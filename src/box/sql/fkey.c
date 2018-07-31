/*
 * Copyright 2010-2017, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This file contains code used by the compiler to add foreign key
 * support to compiled SQL statements.
 */
#include "coll.h"
#include "sqliteInt.h"
#include "box/fkey.h"
#include "box/schema.h"
#include "box/session.h"
#include "tarantoolInt.h"

#ifndef SQLITE_OMIT_FOREIGN_KEY

/*
 * Deferred and Immediate FKs
 * --------------------------
 *
 * Foreign keys in SQLite come in two flavours: deferred and immediate.
 * If an immediate foreign key constraint is violated,
 * SQLITE_CONSTRAINT_FOREIGNKEY is returned and the current
 * statement transaction rolled back. If a
 * deferred foreign key constraint is violated, no action is taken
 * immediately. However if the application attempts to commit the
 * transaction before fixing the constraint violation, the attempt fails.
 *
 * Deferred constraints are implemented using a simple counter associated
 * with the database handle. The counter is set to zero each time a
 * database transaction is opened. Each time a statement is executed
 * that causes a foreign key violation, the counter is incremented. Each
 * time a statement is executed that removes an existing violation from
 * the database, the counter is decremented. When the transaction is
 * committed, the commit fails if the current value of the counter is
 * greater than zero. This scheme has two big drawbacks:
 *
 *   * When a commit fails due to a deferred foreign key constraint,
 *     there is no way to tell which foreign constraint is not satisfied,
 *     or which row it is not satisfied for.
 *
 *   * If the database contains foreign key violations when the
 *     transaction is opened, this may cause the mechanism to malfunction.
 *
 * Despite these problems, this approach is adopted as it seems simpler
 * than the alternatives.
 *
 * INSERT operations:
 *
 *   I.1) For each FK for which the table is the child table, search
 *        the parent table for a match. If none is found increment the
 *        constraint counter.
 *
 *   I.2) For each FK for which the table is the parent table,
 *        search the child table for rows that correspond to the new
 *        row in the parent table. Decrement the counter for each row
 *        found (as the constraint is now satisfied).
 *
 * DELETE operations:
 *
 *   D.1) For each FK for which the table is the child table,
 *        search the parent table for a row that corresponds to the
 *        deleted row in the child table. If such a row is not found,
 *        decrement the counter.
 *
 *   D.2) For each FK for which the table is the parent table, search
 *        the child table for rows that correspond to the deleted row
 *        in the parent table. For each found increment the counter.
 *
 * UPDATE operations:
 *
 *   An UPDATE command requires that all 4 steps above are taken, but only
 *   for FK constraints for which the affected columns are actually
 *   modified (values must be compared at runtime).
 *
 * Note that I.1 and D.1 are very similar operations, as are I.2 and D.2.
 * This simplifies the implementation a bit.
 *
 * For the purposes of immediate FK constraints, the OR REPLACE conflict
 * resolution is considered to delete rows before the new row is inserted.
 * If a delete caused by OR REPLACE violates an FK constraint, an exception
 * is thrown, even if the FK constraint would be satisfied after the new
 * row is inserted.
 *
 * Immediate constraints are usually handled similarly. The only difference
 * is that the counter used is stored as part of each individual statement
 * object (struct Vdbe). If, after the statement has run, its immediate
 * constraint counter is greater than zero,
 * it returns SQLITE_CONSTRAINT_FOREIGNKEY
 * and the statement transaction is rolled back. An exception is an INSERT
 * statement that inserts a single row only (no triggers). In this case,
 * instead of using a counter, an exception is thrown immediately if the
 * INSERT violates a foreign key constraint. This is necessary as such
 * an INSERT does not open a statement transaction.
 *
 * TODO: How should dropping a table be handled? How should renaming a
 * table be handled?
 *
 *
 * Query API Notes
 * ---------------
 *
 * Before coding an UPDATE or DELETE row operation, the code-generator
 * for those two operations needs to know whether or not the operation
 * requires any FK processing and, if so, which columns of the original
 * row are required by the FK processing VDBE code (i.e. if FKs were
 * implemented using triggers, which of the old.* columns would be
 * accessed). No information is required by the code-generator before
 * coding an INSERT operation. The functions used by the UPDATE/DELETE
 * generation code to query for this information are:
 *
 *   sqlite3FkRequired() - Test to see if FK processing is required.
 *   sqlite3FkOldmask()  - Query for the set of required old.* columns.
 *
 *
 * Externally accessible module functions
 * --------------------------------------
 *
 *   sqlite3FkCheck()    - Check for foreign key violations.
 *   sqlite3FkActions()  - Code triggers for ON UPDATE/ON DELETE actions.
 *   sqlite3FkDelete()   - Delete an FKey structure.
 */

/*
 * VDBE Calling Convention
 * -----------------------
 *
 * Example:
 *
 *   For the following INSERT statement:
 *
 *     CREATE TABLE t1(a, b INTEGER PRIMARY KEY, c);
 *     INSERT INTO t1 VALUES(1, 2, 3.1);
 *
 *   Register (x):        2    (type integer)
 *   Register (x+1):      1    (type integer)
 *   Register (x+2):      NULL (type NULL)
 *   Register (x+3):      3.1  (type real)
 */

/*
 * A foreign key constraint requires that the key columns in the parent
 * table are collectively subject to a UNIQUE or PRIMARY KEY constraint.
 * Given that pParent is the parent table for foreign key constraint pFKey,
 * search the schema for a unique index on the parent key columns.
 *
 * If successful, zero is returned. If the parent key is an INTEGER PRIMARY
 * KEY column, then output variable *ppIdx is set to NULL. Otherwise, *ppIdx
 * is set to point to the unique index.
 *
 * If the parent key consists of a single column (the foreign key constraint
 * is not a composite foreign key), output variable *paiCol is set to NULL.
 * Otherwise, it is set to point to an allocated array of size N, where
 * N is the number of columns in the parent key. The first element of the
 * array is the index of the child table column that is mapped by the FK
 * constraint to the parent table column stored in the left-most column
 * of index *ppIdx. The second element of the array is the index of the
 * child table column that corresponds to the second left-most column of
 * *ppIdx, and so on.
 *
 * If the required index cannot be found, either because:
 *
 *   1) The named parent key columns do not exist, or
 *
 *   2) The named parent key columns do exist, but are not subject to a
 *      UNIQUE or PRIMARY KEY constraint, or
 *
 *   3) No parent key columns were provided explicitly as part of the
 *      foreign key definition, and the parent table does not have a
 *      PRIMARY KEY, or
 *
 *   4) No parent key columns were provided explicitly as part of the
 *      foreign key definition, and the PRIMARY KEY of the parent table
 *      consists of a different number of columns to the child key in
 *      the child table.
 *
 * then non-zero is returned, and a "foreign key mismatch" error loaded
 * into pParse. If an OOM error occurs, non-zero is returned and the
 * pParse->db->mallocFailed flag is set.
 */
int
sqlite3FkLocateIndex(Parse * pParse,	/* Parse context to store any error in */
		     Table * pParent,	/* Parent table of FK constraint pFKey */
		     FKey * pFKey,	/* Foreign key to find index for */
		     Index ** ppIdx,	/* OUT: Unique index on parent table */
		     int **paiCol	/* OUT: Map of index columns in pFKey */
    )
{
	int *aiCol = 0;		/* Value to return via *paiCol */
	int nCol = pFKey->nCol;	/* Number of columns in parent key */
	char *zKey = pFKey->aCol[0].zCol;	/* Name of left-most parent key column */

	/* The caller is responsible for zeroing output parameters. */
	assert(ppIdx && *ppIdx == 0);
	assert(!paiCol || *paiCol == 0);
	assert(pParse);

	/* If this is a non-composite (single column) foreign key, check if it
	 * maps to the INTEGER PRIMARY KEY of table pParent. If so, leave *ppIdx
	 * and *paiCol set to zero and return early.
	 *
	 * Otherwise, for a composite foreign key (more than one column), allocate
	 * space for the aiCol array (returned via output parameter *paiCol).
	 * Non-composite foreign keys do not require the aiCol array.
	 */
	if (paiCol && nCol > 1) {
		aiCol =
		    (int *)sqlite3DbMallocRawNN(pParse->db, nCol * sizeof(int));
		if (!aiCol)
			return 1;
		*paiCol = aiCol;
	}

	struct Index *index = NULL;
	for (index = pParent->pIndex; index != NULL; index = index->pNext) {
		int part_count = index->def->key_def->part_count;
		if (part_count != nCol || !index->def->opts.is_unique ||
		    index->pPartIdxWhere != NULL)
			continue;
		/*
		 * Index is a UNIQUE index (or a PRIMARY KEY) and
		 * has the right number of columns. If each
		 * indexed column corresponds to a foreign key
		 * column of pFKey, then this index is a winner.
		 */
		if (zKey == NULL) {
			/*
			 * If zKey is NULL, then this foreign key
			 * is implicitly mapped to the PRIMARY KEY
			 * of table pParent. The PRIMARY KEY index
			 * may be identified by the test.
			 */
			if (IsPrimaryKeyIndex(index)) {
				if (aiCol != NULL) {
					for (int i = 0; i < nCol; i++)
						aiCol[i] = pFKey->aCol[i].iFrom;
				}
				break;
			}
		} else {
			/*
			 * If zKey is non-NULL, then this foreign
			 * key was declared to map to an explicit
			 * list of columns in table pParent. Check
			 * if this index matches those columns.
			 * Also, check that the index uses the
			 * default collation sequences for each
			 * column.
			 */
			int i, j;
			struct key_part *part = index->def->key_def->parts;
			for (i = 0; i < nCol; i++, part++) {
				/*
				 * Index of column in parent
				 * table.
				 */
				i16 iCol = (int) part->fieldno;
				/*
				 * If the index uses a collation
				 * sequence that is different from
				 * the default collation sequence
				 * for the column, this index is
				 * unusable. Bail out early in
				 * this case.
				 */
				uint32_t id;
				struct coll *def_coll =
					sql_column_collation(pParent->def,
							     iCol, &id);
				struct coll *coll = part->coll;
				if (def_coll != coll)
					break;

				char *zIdxCol = pParent->def->fields[iCol].name;
				for (j = 0; j < nCol; j++) {
					if (strcmp(pFKey->aCol[j].zCol,
						   zIdxCol) != 0)
						continue;
					if (aiCol)
						aiCol[i] = pFKey->aCol[j].iFrom;
					break;
				}
				if (j == nCol)
					break;
			}
			if (i == nCol) {
				/* Index is usable. */
				break;
			}
		}
	}

	if (index == NULL) {
		sqlite3ErrorMsg(pParse, "foreign key mismatch - "
					"\"%w\" referencing \"%w\"",
				pFKey->pFrom->def->name, pFKey->zTo);
		}

	*ppIdx = index;
	return 0;
}

/*
 * This function is called when a row is inserted into or deleted from the
 * child table of foreign key constraint pFKey. If an SQL UPDATE is executed
 * on the child table of pFKey, this function is invoked twice for each row
 * affected - once to "delete" the old row, and then again to "insert" the
 * new row.
 *
 * Each time it is called, this function generates VDBE code to locate the
 * row in the parent table that corresponds to the row being inserted into
 * or deleted from the child table. If the parent row can be found, no
 * special action is taken. Otherwise, if the parent row can *not* be
 * found in the parent table:
 *
 *   Operation | FK type   | Action taken
 *   --------------------------------------------------------------------------
 *   INSERT      immediate   Increment the "immediate constraint counter".
 *
 *   DELETE      immediate   Decrement the "immediate constraint counter".
 *
 *   INSERT      deferred    Increment the "deferred constraint counter".
 *
 *   DELETE      deferred    Decrement the "deferred constraint counter".
 *
 * These operations are identified in the comment at the top of this file
 * (fkey.c) as "I.1" and "D.1".
 */
static void
fkLookupParent(Parse * pParse,	/* Parse context */
	       Table * pTab,	/* Parent table of FK pFKey */
	       Index * pIdx,	/* Unique index on parent key columns in pTab */
	       FKey * pFKey,	/* Foreign key constraint */
	       int *aiCol,	/* Map from parent key columns to child table columns */
	       int regData,	/* Address of array containing child table row */
	       int nIncr,	/* Increment constraint counter by this */
	       int isIgnore	/* If true, pretend pTab contains all NULL values */
    )
{
	int i;			/* Iterator variable */
	Vdbe *v = sqlite3GetVdbe(pParse);	/* Vdbe to add code to */
	int iCur = pParse->nTab - 1;	/* Cursor number to use */
	int iOk = sqlite3VdbeMakeLabel(v);	/* jump here if parent key found */
	struct session *user_session = current_session();

	/* If nIncr is less than zero, then check at runtime if there are any
	 * outstanding constraints to resolve. If there are not, there is no need
	 * to check if deleting this row resolves any outstanding violations.
	 *
	 * Check if any of the key columns in the child table row are NULL. If
	 * any are, then the constraint is considered satisfied. No need to
	 * search for a matching row in the parent table.
	 */
	if (nIncr < 0) {
		sqlite3VdbeAddOp2(v, OP_FkIfZero, pFKey->isDeferred, iOk);
		VdbeCoverage(v);
	}
	for (i = 0; i < pFKey->nCol; i++) {
		int iReg = aiCol[i] + regData + 1;
		sqlite3VdbeAddOp2(v, OP_IsNull, iReg, iOk);
		VdbeCoverage(v);
	}

	if (isIgnore == 0) {
		if (pIdx == 0) {
			/* If pIdx is NULL, then the parent key is the INTEGER PRIMARY KEY
			 * column of the parent table (table pTab).
			 */
			int regTemp = sqlite3GetTempReg(pParse);

			/* Invoke MustBeInt to coerce the child key value to an integer (i.e.
			 * apply the affinity of the parent key). If this fails, then there
			 * is no matching parent key. Before using MustBeInt, make a copy of
			 * the value. Otherwise, the value inserted into the child key column
			 * will have INTEGER affinity applied to it, which may not be correct.
			 */
			sqlite3VdbeAddOp2(v, OP_SCopy, aiCol[0] + 1 + regData,
					  regTemp);
			VdbeCoverage(v);

			/* If the parent table is the same as the child table, and we are about
			 * to increment the constraint-counter (i.e. this is an INSERT operation),
			 * then check if the row being inserted matches itself. If so, do not
			 * increment the constraint-counter.
			 */
			if (pTab == pFKey->pFrom && nIncr == 1) {
				sqlite3VdbeAddOp3(v, OP_Eq, regData, iOk,
						  regTemp);
				VdbeCoverage(v);
				sqlite3VdbeChangeP5(v, SQLITE_NOTNULL);
			}

		} else {
			int nCol = pFKey->nCol;
			int regTemp = sqlite3GetTempRange(pParse, nCol);
			int regRec = sqlite3GetTempReg(pParse);
			struct space *space =
				space_by_id(pIdx->pTable->def->id);
			vdbe_emit_open_cursor(pParse, iCur, pIdx->def->iid,
					      space);
			for (i = 0; i < nCol; i++) {
				sqlite3VdbeAddOp2(v, OP_Copy,
						  aiCol[i] + 1 + regData,
						  regTemp + i);
			}

			/* If the parent table is the same as the child table, and we are about
			 * to increment the constraint-counter (i.e. this is an INSERT operation),
			 * then check if the row being inserted matches itself. If so, do not
			 * increment the constraint-counter.
			 *
			 * If any of the parent-key values are NULL, then the row cannot match
			 * itself. So set JUMPIFNULL to make sure we do the OP_Found if any
			 * of the parent-key values are NULL (at this point it is known that
			 * none of the child key values are).
			 */
			if (pTab == pFKey->pFrom && nIncr == 1) {
				int iJump =
					sqlite3VdbeCurrentAddr(v) + nCol + 1;
				struct key_part *part =
					pIdx->def->key_def->parts;
				for (i = 0; i < nCol; ++i, ++part) {
					int iChild = aiCol[i] + 1 + regData;
					int iParent = 1 + regData +
						      (int)part->fieldno;
					sqlite3VdbeAddOp3(v, OP_Ne, iChild,
							  iJump, iParent);
					VdbeCoverage(v);
					sqlite3VdbeChangeP5(v,
							    SQLITE_JUMPIFNULL);
				}
				sqlite3VdbeGoto(v, iOk);
			}

			sqlite3VdbeAddOp4(v, OP_MakeRecord, regTemp, nCol,
					  regRec,
					  sqlite3IndexAffinityStr(pParse->db,
								  pIdx), nCol);
			sqlite3VdbeAddOp4Int(v, OP_Found, iCur, iOk, regRec, 0);
			VdbeCoverage(v);

			sqlite3ReleaseTempReg(pParse, regRec);
			sqlite3ReleaseTempRange(pParse, regTemp, nCol);
		}
	}

	if (!pFKey->isDeferred && !(user_session->sql_flags & SQLITE_DeferFKs)
	    && !pParse->pToplevel && !pParse->isMultiWrite) {
		/* Special case: If this is an INSERT statement that will insert exactly
		 * one row into the table, raise a constraint immediately instead of
		 * incrementing a counter. This is necessary as the VM code is being
		 * generated for will not open a statement transaction.
		 */
		assert(nIncr == 1);
		sqlite3HaltConstraint(pParse, SQLITE_CONSTRAINT_FOREIGNKEY,
				      ON_CONFLICT_ACTION_ABORT, 0, P4_STATIC,
				      P5_ConstraintFK);
	} else {
		if (nIncr > 0 && pFKey->isDeferred == 0) {
			sqlite3MayAbort(pParse);
		}
		sqlite3VdbeAddOp2(v, OP_FkCounter, pFKey->isDeferred, nIncr);
	}

	sqlite3VdbeResolveLabel(v, iOk);
	sqlite3VdbeAddOp1(v, OP_Close, iCur);
}

/*
 * Return an Expr object that refers to a memory register corresponding
 * to column iCol of table pTab.
 *
 * regBase is the first of an array of register that contains the data
 * for pTab.  regBase+1 holds the first column.
 * regBase+2 holds the second column, and so forth.
 */
static Expr *
exprTableRegister(Parse * pParse,	/* Parsing and code generating context */
		  Table * pTab,	/* The table whose content is at r[regBase]... */
		  int regBase,	/* Contents of table pTab */
		  i16 iCol	/* Which column of pTab is desired */
    )
{
	Expr *pExpr;
	sqlite3 *db = pParse->db;

	pExpr = sqlite3Expr(db, TK_REGISTER, 0);
	if (pExpr) {
		if (iCol >= 0) {
			pExpr->iTable = regBase + iCol + 1;
			char affinity = pTab->def->fields[iCol].affinity;
			pExpr->affinity = affinity;
			pExpr = sqlite3ExprAddCollateString(pParse, pExpr,
							    "binary");
		} else {
			pExpr->iTable = regBase;
			pExpr->affinity = AFFINITY_INTEGER;
		}
	}
	return pExpr;
}

/**
 * Return an Expr object that refers to column of space_def which
 * has cursor cursor.
 * @param db The database connection.
 * @param def space definition.
 * @param cursor The open cursor on the table.
 * @param column The column that is wanted.
 * @retval not NULL on success.
 * @retval NULL on error.
 */
static Expr *
exprTableColumn(sqlite3 * db, struct space_def *def, int cursor, i16 column)
{
	Expr *pExpr = sqlite3Expr(db, TK_COLUMN, 0);
	if (pExpr) {
		pExpr->space_def = def;
		pExpr->iTable = cursor;
		pExpr->iColumn = column;
	}
	return pExpr;
}

/*
 * This function is called to generate code executed when a row is deleted
 * from the parent table of foreign key constraint pFKey and, if pFKey is
 * deferred, when a row is inserted into the same table. When generating
 * code for an SQL UPDATE operation, this function may be called twice -
 * once to "delete" the old row and once to "insert" the new row.
 *
 * Parameter nIncr is passed -1 when inserting a row (as this may decrease
 * the number of FK violations in the db) or +1 when deleting one (as this
 * may increase the number of FK constraint problems).
 *
 * The code generated by this function scans through the rows in the child
 * table that correspond to the parent table row being deleted or inserted.
 * For each child row found, one of the following actions is taken:
 *
 *   Operation | FK type   | Action taken
 *   --------------------------------------------------------------------------
 *   DELETE      immediate   Increment the "immediate constraint counter".
 *                           Or, if the ON (UPDATE|DELETE) action is RESTRICT,
 *                           throw a "FOREIGN KEY constraint failed" exception.
 *
 *   INSERT      immediate   Decrement the "immediate constraint counter".
 *
 *   DELETE      deferred    Increment the "deferred constraint counter".
 *                           Or, if the ON (UPDATE|DELETE) action is RESTRICT,
 *                           throw a "FOREIGN KEY constraint failed" exception.
 *
 *   INSERT      deferred    Decrement the "deferred constraint counter".
 *
 * These operations are identified in the comment at the top of this file
 * (fkey.c) as "I.2" and "D.2".
 */
static void
fkScanChildren(Parse * pParse,	/* Parse context */
	       SrcList * pSrc,	/* The child table to be scanned */
	       Table * pTab,	/* The parent table */
	       Index * pIdx,	/* Index on parent covering the foreign key */
	       FKey * pFKey,	/* The foreign key linking pSrc to pTab */
	       int *aiCol,	/* Map from pIdx cols to child table cols */
	       int regData,	/* Parent row data starts here */
	       int nIncr	/* Amount to increment deferred counter by */
    )
{
	sqlite3 *db = pParse->db;	/* Database handle */
	Expr *pWhere = 0;	/* WHERE clause to scan with */
	NameContext sNameContext;	/* Context used to resolve WHERE clause */
	WhereInfo *pWInfo;	/* Context used by sqlite3WhereXXX() */
	int iFkIfZero = 0;	/* Address of OP_FkIfZero */
	Vdbe *v = sqlite3GetVdbe(pParse);

	assert(pIdx == NULL || pIdx->pTable == pTab);
	assert(pIdx == NULL || (int) pIdx->def->key_def->part_count == pFKey->nCol);
	assert(pIdx != NULL);

	if (nIncr < 0) {
		iFkIfZero =
		    sqlite3VdbeAddOp2(v, OP_FkIfZero, pFKey->isDeferred, 0);
		VdbeCoverage(v);
	}

	/* Create an Expr object representing an SQL expression like:
	 *
	 *   <parent-key1> = <child-key1> AND <parent-key2> = <child-key2> ...
	 *
	 * The collation sequence used for the comparison should be that of
	 * the parent key columns. The affinity of the parent key column should
	 * be applied to each child key value before the comparison takes place.
	 */
	for (int i = 0; i < pFKey->nCol; i++) {
		Expr *pLeft;	/* Value from parent table row */
		Expr *pRight;	/* Column ref to child table */
		Expr *pEq;	/* Expression (pLeft = pRight) */
		i16 iCol;	/* Index of column in child table */
		const char *column_name;

		iCol = pIdx != NULL ?
		       (int) pIdx->def->key_def->parts[i].fieldno : -1;
		pLeft = exprTableRegister(pParse, pTab, regData, iCol);
		iCol = aiCol ? aiCol[i] : pFKey->aCol[0].iFrom;
		assert(iCol >= 0);
		column_name = pFKey->pFrom->def->fields[iCol].name;
		pRight = sqlite3Expr(db, TK_ID, column_name);
		pEq = sqlite3PExpr(pParse, TK_EQ, pLeft, pRight);
		pWhere = sqlite3ExprAnd(db, pWhere, pEq);
	}

	/* If the child table is the same as the parent table, then add terms
	 * to the WHERE clause that prevent this entry from being scanned.
	 * The added WHERE clause terms are like this:
	 *
	 *     NOT( $current_a==a AND $current_b==b AND ... )
	 *     The primary key is (a,b,...)
	 */
	if (pTab == pFKey->pFrom && nIncr > 0) {
		Expr *pNe;	/* Expression (pLeft != pRight) */
		Expr *pLeft;	/* Value from parent table row */
		Expr *pRight;	/* Column ref to child table */

		Expr *pEq, *pAll = 0;
		Index *pPk = sqlite3PrimaryKeyIndex(pTab);
		assert(pIdx != NULL);
		uint32_t part_count = pPk->def->key_def->part_count;
		for (uint32_t i = 0; i < part_count; i++) {
			uint32_t fieldno = pIdx->def->key_def->parts[i].fieldno;
			pLeft = exprTableRegister(pParse, pTab, regData,
						  fieldno);
			pRight = exprTableColumn(db, pTab->def,
						 pSrc->a[0].iCursor, fieldno);
			pEq = sqlite3PExpr(pParse, TK_EQ, pLeft, pRight);
			pAll = sqlite3ExprAnd(db, pAll, pEq);
		}
		pNe = sqlite3PExpr(pParse, TK_NOT, pAll, 0);
		pWhere = sqlite3ExprAnd(db, pWhere, pNe);
	}

	/* Resolve the references in the WHERE clause. */
	memset(&sNameContext, 0, sizeof(NameContext));
	sNameContext.pSrcList = pSrc;
	sNameContext.pParse = pParse;
	sqlite3ResolveExprNames(&sNameContext, pWhere);

	/* Create VDBE to loop through the entries in pSrc that match the WHERE
	 * clause. For each row found, increment either the deferred or immediate
	 * foreign key constraint counter.
	 */
	pWInfo = sqlite3WhereBegin(pParse, pSrc, pWhere, 0, 0, 0, 0);
	sqlite3VdbeAddOp2(v, OP_FkCounter, pFKey->isDeferred, nIncr);
	if (pWInfo) {
		sqlite3WhereEnd(pWInfo);
	}

	/* Clean up the WHERE clause constructed above. */
	sql_expr_delete(db, pWhere, false);
	if (iFkIfZero)
		sqlite3VdbeJumpHere(v, iFkIfZero);
}

/*
 * This function returns a linked list of FKey objects (connected by
 * FKey.pNextTo) holding all children of table pTab.  For example,
 * given the following schema:
 *
 *   CREATE TABLE t1(a PRIMARY KEY);
 *   CREATE TABLE t2(b REFERENCES t1(a);
 *
 * Calling this function with table "t1" as an argument returns a pointer
 * to the FKey structure representing the foreign key constraint on table
 * "t2". Calling this function with "t2" as the argument would return a
 * NULL pointer (as there are no FK constraints for which t2 is the parent
 * table).
 */
FKey *
sqlite3FkReferences(Table * pTab)
{
	return (FKey *) sqlite3HashFind(&pTab->pSchema->fkeyHash,
					pTab->def->name);
}

/*
 * The second argument points to an FKey object representing a foreign key
 * for which pTab is the child table. An UPDATE statement against pTab
 * is currently being processed. For each column of the table that is
 * actually updated, the corresponding element in the aChange[] array
 * is zero or greater (if a column is unmodified the corresponding element
 * is set to -1).
 *
 * This function returns true if any of the columns that are part of the
 * child key for FK constraint *p are modified.
 */
static int
fkChildIsModified(FKey * p,	/* Foreign key for which pTab is the child */
		  int *aChange	/* Array indicating modified columns */
    )
{
	int i;
	for (i = 0; i < p->nCol; i++) {
		int iChildKey = p->aCol[i].iFrom;
		if (aChange[iChildKey] >= 0)
			return 1;
	}
	return 0;
}

/*
 * The second argument points to an FKey object representing a foreign key
 * for which pTab is the parent table. An UPDATE statement against pTab
 * is currently being processed. For each column of the table that is
 * actually updated, the corresponding element in the aChange[] array
 * is zero or greater (if a column is unmodified the corresponding element
 * is set to -1).
 *
 * This function returns true if any of the columns that are part of the
 * parent key for FK constraint *p are modified.
 */
static int
fkParentIsModified(Table * pTab, FKey * p, int *aChange)
{
	int i;
	for (i = 0; i < p->nCol; i++) {
		char *zKey = p->aCol[i].zCol;
		int iKey;
		for (iKey = 0; iKey < (int)pTab->def->field_count; iKey++) {
			if (aChange[iKey] >= 0) {
				if (zKey) {
					if (strcmp(pTab->def->fields[iKey].name,
						   zKey) == 0)
						return 1;
				} else if (table_column_is_in_pk(pTab, iKey)) {
					return 1;
				}
			}
		}
	}
	return 0;
}

/*
 * Return true if the parser passed as the first argument is being
 * used to code a trigger that is really a "SET NULL" action belonging
 * to trigger pFKey.
 */
static int
isSetNullAction(Parse * pParse, FKey * pFKey)
{
	Parse *pTop = sqlite3ParseToplevel(pParse);
	if (pTop->pTriggerPrg != NULL) {
		struct sql_trigger *trigger = pTop->pTriggerPrg->trigger;
		if ((trigger == pFKey->apTrigger[0] &&
		     pFKey->aAction[0] == OE_SetNull) ||
		    (trigger == pFKey->apTrigger[1]
			&& pFKey->aAction[1] == OE_SetNull))
			return 1;
	}
	return 0;
}

/*
 * This function is called when inserting, deleting or updating a row of
 * table pTab to generate VDBE code to perform foreign key constraint
 * processing for the operation.
 *
 * For a DELETE operation, parameter regOld is passed the index of the
 * first register in an array of (pTab->nCol+1) registers containing the
 * PK of the row being deleted, followed by each of the column values
 * of the row being deleted, from left to right. Parameter regNew is passed
 * zero in this case.
 *
 * For an INSERT operation, regOld is passed zero and regNew is passed the
 * first register of an array of (pTab->nCol+1) registers containing the new
 * row data.
 *
 * For an UPDATE operation, this function is called twice. Once before
 * the original record is deleted from the table using the calling convention
 * described for DELETE. Then again after the original record is deleted
 * but before the new record is inserted using the INSERT convention.
 */
void
sqlite3FkCheck(Parse * pParse,	/* Parse context */
	       Table * pTab,	/* Row is being deleted from this table */
	       int regOld,	/* Previous row data is stored here */
	       int regNew,	/* New row data is stored here */
	       int *aChange	/* Array indicating UPDATEd columns (or 0) */
    )
{
	sqlite3 *db = pParse->db;	/* Database handle */
	FKey *pFKey;		/* Used to iterate through FKs */
	struct session *user_session = current_session();

	/* Exactly one of regOld and regNew should be non-zero. */
	assert((regOld == 0) != (regNew == 0));

	/* If foreign-keys are disabled, this function is a no-op. */
	if ((user_session->sql_flags & SQLITE_ForeignKeys) == 0)
		return;

	/* Loop through all the foreign key constraints for which pTab is the
	 * child table (the table that the foreign key definition is part of).
	 */
	for (pFKey = pTab->pFKey; pFKey; pFKey = pFKey->pNextFrom) {
		Table *pTo;	/* Parent table of foreign key pFKey */
		Index *pIdx = 0;	/* Index on key columns in pTo */
		int *aiFree = 0;
		int *aiCol;
		int iCol;
		int bIgnore = 0;

		if (aChange
		    && sqlite3_stricmp(pTab->def->name, pFKey->zTo) != 0
		    && fkChildIsModified(pFKey, aChange) == 0) {
			continue;
		}

		/* Find the parent table of this foreign key. Also find a unique index
		 * on the parent key columns in the parent table. If either of these
		 * schema items cannot be located, set an error in pParse and return
		 * early.
		 */
		pTo = sqlite3LocateTable(pParse, 0, pFKey->zTo);
		if (pTo == NULL || sqlite3FkLocateIndex(pParse, pTo, pFKey,
							&pIdx, &aiFree) != 0)
				return;
		assert(pFKey->nCol == 1 || (aiFree && pIdx));

		if (aiFree) {
			aiCol = aiFree;
		} else {
			iCol = pFKey->aCol[0].iFrom;
			aiCol = &iCol;
		}

		pParse->nTab++;

		if (regOld != 0) {
			/* A row is being removed from the child table. Search for the parent.
			 * If the parent does not exist, removing the child row resolves an
			 * outstanding foreign key constraint violation.
			 */
			fkLookupParent(pParse, pTo, pIdx, pFKey, aiCol,
				       regOld, -1, bIgnore);
		}
		if (regNew != 0 && !isSetNullAction(pParse, pFKey)) {
			/* A row is being added to the child table. If a parent row cannot
			 * be found, adding the child row has violated the FK constraint.
			 *
			 * If this operation is being performed as part of a trigger program
			 * that is actually a "SET NULL" action belonging to this very
			 * foreign key, then omit this scan altogether. As all child key
			 * values are guaranteed to be NULL, it is not possible for adding
			 * this row to cause an FK violation.
			 */
			fkLookupParent(pParse, pTo, pIdx, pFKey, aiCol,
				       regNew, +1, bIgnore);
		}

		sqlite3DbFree(db, aiFree);
	}

	/* Loop through all the foreign key constraints that refer to this table.
	 * (the "child" constraints)
	 */
	for (pFKey = sqlite3FkReferences(pTab); pFKey; pFKey = pFKey->pNextTo) {
		Index *pIdx = 0;	/* Foreign key index for pFKey */
		SrcList *pSrc;
		int *aiCol = 0;

		if (aChange
		    && fkParentIsModified(pTab, pFKey, aChange) == 0) {
			continue;
		}

		if (!pFKey->isDeferred
		    && !(user_session->sql_flags & SQLITE_DeferFKs)
		    && !pParse->pToplevel && !pParse->isMultiWrite) {
			assert(regOld == 0 && regNew != 0);
			/* Inserting a single row into a parent table cannot cause (or fix)
			 * an immediate foreign key violation. So do nothing in this case.
			 */
			continue;
		}

		if (sqlite3FkLocateIndex(pParse, pTab, pFKey, &pIdx,
					 &aiCol) != 0)
			return;
		assert(aiCol || pFKey->nCol == 1);

		/* Create a SrcList structure containing the child table.  We need the
		 * child table as a SrcList for sqlite3WhereBegin()
		 */
		pSrc = sqlite3SrcListAppend(db, 0, 0);
		if (pSrc) {
			struct SrcList_item *pItem = pSrc->a;
			pItem->pTab = pFKey->pFrom;
			pItem->zName = pFKey->pFrom->def->name;
			pItem->pTab->nTabRef++;
			pItem->iCursor = pParse->nTab++;

			if (regNew != 0) {
				fkScanChildren(pParse, pSrc, pTab, pIdx, pFKey,
					       aiCol, regNew, -1);
			}
			if (regOld != 0) {
				int eAction = pFKey->aAction[aChange != 0];
				fkScanChildren(pParse, pSrc, pTab, pIdx, pFKey,
					       aiCol, regOld, 1);
				/* If this is a deferred FK constraint, or a CASCADE or SET NULL
				 * action applies, then any foreign key violations caused by
				 * removing the parent key will be rectified by the action trigger.
				 * So do not set the "may-abort" flag in this case.
				 *
				 * Note 1: If the FK is declared "ON UPDATE CASCADE", then the
				 * may-abort flag will eventually be set on this statement anyway
				 * (when this function is called as part of processing the UPDATE
				 * within the action trigger).
				 *
				 * Note 2: At first glance it may seem like SQLite could simply omit
				 * all OP_FkCounter related scans when either CASCADE or SET NULL
				 * applies. The trouble starts if the CASCADE or SET NULL action
				 * trigger causes other triggers or action rules attached to the
				 * child table to fire. In these cases the fk constraint counters
				 * might be set incorrectly if any OP_FkCounter related scans are
				 * omitted.
				 */
				if (!pFKey->isDeferred && eAction != OE_Cascade
				    && eAction != OE_SetNull) {
					sqlite3MayAbort(pParse);
				}
			}
			pItem->zName = 0;
			sqlite3SrcListDelete(db, pSrc);
		}
		sqlite3DbFree(db, aiCol);
	}
}

#define COLUMN_MASK(x) (((x)>31) ? 0xffffffff : ((u32)1<<(x)))

/*
 * This function is called before generating code to update or delete a
 * row contained in table pTab.
 */
u32
sqlite3FkOldmask(Parse * pParse,	/* Parse context */
		 Table * pTab	/* Table being modified */
    )
{
	u32 mask = 0;
	struct session *user_session = current_session();

	if (user_session->sql_flags & SQLITE_ForeignKeys) {
		FKey *p;
		for (p = pTab->pFKey; p; p = p->pNextFrom) {
			for (int i = 0; i < p->nCol; i++)
				mask |= COLUMN_MASK(p->aCol[i].iFrom);
		}
		for (p = sqlite3FkReferences(pTab); p; p = p->pNextTo) {
			Index *pIdx = 0;
			sqlite3FkLocateIndex(pParse, pTab, p, &pIdx, 0);
			if (pIdx != NULL) {
				uint32_t part_count =
					pIdx->def->key_def->part_count;
				for (uint32_t i = 0; i < part_count; i++) {
					mask |= COLUMN_MASK(pIdx->def->
						key_def->parts[i].fieldno);
				}
			}
		}
	}
	return mask;
}

/*
 * This function is called before generating code to update or delete a
 * row contained in table pTab. If the operation is a DELETE, then
 * parameter aChange is passed a NULL value. For an UPDATE, aChange points
 * to an array of size N, where N is the number of columns in table pTab.
 * If the i'th column is not modified by the UPDATE, then the corresponding
 * entry in the aChange[] array is set to -1. If the column is modified,
 * the value is 0 or greater.
 *
 * If any foreign key processing will be required, this function returns
 * true. If there is no foreign key related processing, this function
 * returns false.
 */
int
sqlite3FkRequired(Table * pTab,	/* Table being modified */
		  int *aChange	/* Non-NULL for UPDATE operations */
    )
{
	struct session *user_session = current_session();
	if (user_session->sql_flags & SQLITE_ForeignKeys) {
		if (!aChange) {
			/* A DELETE operation. Foreign key processing is required if the
			 * table in question is either the child or parent table for any
			 * foreign key constraint.
			 */
			return (sqlite3FkReferences(pTab) || pTab->pFKey);
		} else {
			/* This is an UPDATE. Foreign key processing is only required if the
			 * operation modifies one or more child or parent key columns.
			 */
			FKey *p;

			/* Check if any child key columns are being modified. */
			for (p = pTab->pFKey; p; p = p->pNextFrom) {
				if (fkChildIsModified(p, aChange))
					return 1;
			}

			/* Check if any parent key columns are being modified. */
			for (p = sqlite3FkReferences(pTab); p; p = p->pNextTo) {
				if (fkParentIsModified(pTab, p, aChange))
					return 1;
			}
		}
	}
	return 0;
}

/**
 * This function is called when an UPDATE or DELETE operation is
 * being compiled on table pTab, which is the parent table of
 * foreign-key pFKey.
 * If the current operation is an UPDATE, then the pChanges
 * parameter is passed a pointer to the list of columns being
 * modified. If it is a DELETE, pChanges is passed a NULL pointer.
 *
 * It returns a pointer to a sql_trigger structure containing a
 * trigger equivalent to the ON UPDATE or ON DELETE action
 * specified by pFKey.
 * If the action is "NO ACTION" or "RESTRICT", then a NULL pointer
 * is returned (these actions require no special handling by the
 * triggers sub-system, code for them is created by
 * fkScanChildren()).
 *
 * For example, if pFKey is the foreign key and pTab is table "p"
 * in the following schema:
 *
 *   CREATE TABLE p(pk PRIMARY KEY);
 *   CREATE TABLE c(ck REFERENCES p ON DELETE CASCADE);
 *
 * then the returned trigger structure is equivalent to:
 *
 *   CREATE TRIGGER ... DELETE ON p BEGIN
 *     DELETE FROM c WHERE ck = old.pk;
 *   END;
 *
 * The returned pointer is cached as part of the foreign key
 * object. It is eventually freed along with the rest of the
 * foreign key object by sqlite3FkDelete().
 *
 * @param pParse Parse context.
 * @param pTab Table being updated or deleted from.
 * @param pFKey Foreign key to get action for.
 * @param pChanges Change-list for UPDATE, NULL for DELETE.
 *
 * @retval not NULL on success.
 * @retval NULL on failure.
 */
static struct sql_trigger *
fkActionTrigger(struct Parse *pParse, struct Table *pTab, struct FKey *pFKey,
		struct ExprList *pChanges)
{
	sqlite3 *db = pParse->db;	/* Database handle */
	int action;		/* One of OE_None, OE_Cascade etc. */
	/* Trigger definition to return. */
	struct sql_trigger *trigger;
	int iAction = (pChanges != 0);	/* 1 for UPDATE, 0 for DELETE */
	struct session *user_session = current_session();

	action = pFKey->aAction[iAction];
	if (action == OE_Restrict
	    && (user_session->sql_flags & SQLITE_DeferFKs)) {
		return 0;
	}
	trigger = pFKey->apTrigger[iAction];

	if (action != ON_CONFLICT_ACTION_NONE && trigger == NULL) {
		char const *zFrom;	/* Name of child table */
		int nFrom;	/* Length in bytes of zFrom */
		Index *pIdx = 0;	/* Parent key index for this FK */
		int *aiCol = 0;	/* child table cols -> parent key cols */
		TriggerStep *pStep = 0;	/* First (only) step of trigger program */
		Expr *pWhere = 0;	/* WHERE clause of trigger step */
		ExprList *pList = 0;	/* Changes list if ON UPDATE CASCADE */
		Select *pSelect = 0;	/* If RESTRICT, "SELECT RAISE(...)" */
		int i;		/* Iterator variable */
		Expr *pWhen = 0;	/* WHEN clause for the trigger */

		if (sqlite3FkLocateIndex(pParse, pTab, pFKey, &pIdx, &aiCol))
			return 0;
		assert(aiCol || pFKey->nCol == 1);

		for (i = 0; i < pFKey->nCol; i++) {
			Token tOld = { "old", 3, false };	/* Literal "old" token */
			Token tNew = { "new", 3, false };	/* Literal "new" token */
			Token tFromCol;	/* Name of column in child table */
			Token tToCol;	/* Name of column in parent table */
			int iFromCol;	/* Idx of column in child table */
			Expr *pEq;	/* tFromCol = OLD.tToCol */

			iFromCol = aiCol ? aiCol[i] : pFKey->aCol[0].iFrom;
			assert(iFromCol >= 0);
			assert(pIdx != NULL);

			uint32_t fieldno = pIdx->def->key_def->parts[i].fieldno;
			sqlite3TokenInit(&tToCol,
					 pTab->def->fields[fieldno].name);
			sqlite3TokenInit(&tFromCol,
					 pFKey->pFrom->def->fields[
						iFromCol].name);

			/* Create the expression "OLD.zToCol = zFromCol". It is important
			 * that the "OLD.zToCol" term is on the LHS of the = operator, so
			 * that the affinity and collation sequence associated with the
			 * parent table are used for the comparison.
			 */
			pEq = sqlite3PExpr(pParse, TK_EQ,
					   sqlite3PExpr(pParse, TK_DOT,
							sqlite3ExprAlloc(db,
									 TK_ID,
									 &tOld,
									 0),
							sqlite3ExprAlloc(db,
									 TK_ID,
									 &tToCol,
									 0)),
					   sqlite3ExprAlloc(db, TK_ID,
							    &tFromCol, 0)
			    );
			pWhere = sqlite3ExprAnd(db, pWhere, pEq);

			/* For ON UPDATE, construct the next term of the WHEN clause.
			 * The final WHEN clause will be like this:
			 *
			 *    WHEN NOT(old.col1 = new.col1 AND ... AND old.colN = new.colN)
			 */
			if (pChanges) {
				pEq = sqlite3PExpr(pParse, TK_EQ,
						   sqlite3PExpr(pParse, TK_DOT,
								sqlite3ExprAlloc
								(db, TK_ID,
								 &tOld, 0),
								sqlite3ExprAlloc
								(db, TK_ID,
								 &tToCol, 0)),
						   sqlite3PExpr(pParse, TK_DOT,
								sqlite3ExprAlloc
								(db, TK_ID,
								 &tNew, 0),
								sqlite3ExprAlloc
								(db, TK_ID,
								 &tToCol, 0))
				    );
				pWhen = sqlite3ExprAnd(db, pWhen, pEq);
			}

			if (action != OE_Restrict
			    && (action != OE_Cascade || pChanges)) {
				Expr *pNew;
				if (action == OE_Cascade) {
					pNew = sqlite3PExpr(pParse, TK_DOT,
							    sqlite3ExprAlloc(db,
									     TK_ID,
									     &tNew,
									     0),
							    sqlite3ExprAlloc(db,
									     TK_ID,
									     &tToCol,
									     0));
				} else if (action == OE_SetDflt) {
					Expr *pDflt =
						space_column_default_expr(
							pFKey->pFrom->def->id,
							(uint32_t)iFromCol);
					if (pDflt) {
						pNew =
						    sqlite3ExprDup(db, pDflt,
								   0);
					} else {
						pNew =
						    sqlite3ExprAlloc(db,
								     TK_NULL, 0,
								     0);
					}
				} else {
					pNew =
					    sqlite3ExprAlloc(db, TK_NULL, 0, 0);
				}
				pList =
				    sql_expr_list_append(pParse->db, pList,
							 pNew);
				sqlite3ExprListSetName(pParse, pList, &tFromCol,
						       0);
			}
		}
		sqlite3DbFree(db, aiCol);

		zFrom = pFKey->pFrom->def->name;
		nFrom = sqlite3Strlen30(zFrom);

		if (action == OE_Restrict) {
			Token tFrom;
			Expr *pRaise;

			tFrom.z = zFrom;
			tFrom.n = nFrom;
			pRaise =
			    sqlite3Expr(db, TK_RAISE,
					"FOREIGN KEY constraint failed");
			if (pRaise) {
				pRaise->affinity = ON_CONFLICT_ACTION_ABORT;
			}
			pSelect = sqlite3SelectNew(pParse,
						   sql_expr_list_append(pParse->db,
									NULL,
									pRaise),
						   sqlite3SrcListAppend(db, 0,
									&tFrom),
						   pWhere, 0, 0, 0, 0, 0, 0);
			pWhere = 0;
		}
		trigger = (struct sql_trigger *)sqlite3DbMallocZero(db,
								    sizeof(*trigger));
		if (trigger != NULL) {
			size_t step_size = sizeof(TriggerStep) + nFrom + 1;
			trigger->step_list = sqlite3DbMallocZero(db, step_size);
			pStep = trigger->step_list;
			pStep->zTarget = (char *)&pStep[1];
			memcpy(pStep->zTarget, zFrom, nFrom);
			pStep->pWhere =
			    sqlite3ExprDup(db, pWhere, EXPRDUP_REDUCE);
			pStep->pExprList =
			    sql_expr_list_dup(db, pList, EXPRDUP_REDUCE);
			pStep->pSelect =
			    sqlite3SelectDup(db, pSelect, EXPRDUP_REDUCE);
			if (pWhen) {
				pWhen = sqlite3PExpr(pParse, TK_NOT, pWhen, 0);
				trigger->pWhen =
				    sqlite3ExprDup(db, pWhen, EXPRDUP_REDUCE);
			}
		}

		sql_expr_delete(db, pWhere, false);
		sql_expr_delete(db, pWhen, false);
		sql_expr_list_delete(db, pList);
		sql_select_delete(db, pSelect);
		if (db->mallocFailed == 1) {
			sql_trigger_delete(db, trigger);
			return 0;
		}
		assert(pStep != 0);

		switch (action) {
		case OE_Restrict:
			pStep->op = TK_SELECT;
			break;
		case OE_Cascade:
			if (!pChanges) {
				pStep->op = TK_DELETE;
				break;
			}
			FALLTHROUGH;
		default:
			pStep->op = TK_UPDATE;
		}
		pStep->trigger = trigger;
		pFKey->apTrigger[iAction] = trigger;
		trigger->op = pChanges ? TK_UPDATE : TK_DELETE;
	}

	return trigger;
}

/*
 * This function is called when deleting or updating a row to implement
 * any required CASCADE, SET NULL or SET DEFAULT actions.
 */
void
sqlite3FkActions(Parse * pParse,	/* Parse context */
		 Table * pTab,	/* Table being updated or deleted from */
		 ExprList * pChanges,	/* Change-list for UPDATE, NULL for DELETE */
		 int regOld,	/* Address of array containing old row */
		 int *aChange	/* Array indicating UPDATEd columns (or 0) */
    )
{
	struct session *user_session = current_session();
	/* If foreign-key support is enabled, iterate through all FKs that
	 * refer to table pTab. If there is an action associated with the FK
	 * for this operation (either update or delete), invoke the associated
	 * trigger sub-program.
	 */
	if (user_session->sql_flags & SQLITE_ForeignKeys) {
		FKey *pFKey;	/* Iterator variable */
		for (pFKey = sqlite3FkReferences(pTab); pFKey;
		     pFKey = pFKey->pNextTo) {
			if (aChange == 0
			    || fkParentIsModified(pTab, pFKey, aChange)) {
				struct sql_trigger *pAct =
					fkActionTrigger(pParse, pTab, pFKey,
							pChanges);
				if (pAct == NULL)
					continue;
				vdbe_code_row_trigger_direct(pParse, pAct, pTab,
							     regOld,
							     ON_CONFLICT_ACTION_ABORT,
							     0);
			}
		}
	}
}

/*
 * Free all memory associated with foreign key definitions attached to
 * table pTab. Remove the deleted foreign keys from the Schema.fkeyHash
 * hash table.
 */
void
sqlite3FkDelete(sqlite3 * db, Table * pTab)
{
	FKey *pFKey;		/* Iterator variable */
	FKey *pNext;		/* Copy of pFKey->pNextFrom */

	for (pFKey = pTab->pFKey; pFKey; pFKey = pNext) {
		/* Remove the FK from the fkeyHash hash table. */
		if (!db || db->pnBytesFreed == 0) {
			if (pFKey->pPrevTo) {
				pFKey->pPrevTo->pNextTo = pFKey->pNextTo;
			} else {
				void *p = (void *)pFKey->pNextTo;
				const char *z =
				    (p ? pFKey->pNextTo->zTo : pFKey->zTo);
				sqlite3HashInsert(&pTab->pSchema->fkeyHash, z,
						  p);
			}
			if (pFKey->pNextTo) {
				pFKey->pNextTo->pPrevTo = pFKey->pPrevTo;
			}
		}

		/* EV: R-30323-21917 Each foreign key constraint in SQLite is
		 * classified as either immediate or deferred.
		 */
		assert(pFKey->isDeferred == 0 || pFKey->isDeferred == 1);

		/* Delete any triggers created to implement actions for this FK. */
		sql_trigger_delete(db, pFKey->apTrigger[0]);
		sql_trigger_delete(db, pFKey->apTrigger[1]);

		pNext = pFKey->pNextFrom;
		sqlite3DbFree(db, pFKey);
	}
}
#endif				/* ifndef SQLITE_OMIT_FOREIGN_KEY */
