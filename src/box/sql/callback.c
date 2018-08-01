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
 *
 * This file contains functions used to access the internal hash tables
 * of user defined functions and collation sequences.
 */

#include "box/coll_id_cache.h"
#include "sqliteInt.h"
#include "box/session.h"

struct coll *
sql_get_coll_seq(Parse *parser, const char *name, uint32_t *coll_id)
{
	if (name == NULL || strcasecmp(name, "binary") == 0) {
		*coll_id = COLL_NONE;
		return NULL;
	}
	struct coll_id *p = coll_by_name(name, strlen(name));
	if (p == NULL) {
		*coll_id = COLL_NONE;
		sqlite3ErrorMsg(parser, "no such collation sequence: %s",
				name);
		return NULL;
	} else {
		*coll_id = p->id;
		return p->coll;
	}
}

/* During the search for the best function definition, this procedure
 * is called to test how well the function passed as the first argument
 * matches the request for a function with nArg arguments in a system
 * that uses encoding enc. The value returned indicates how well the
 * request is matched. A higher value indicates a better match.
 *
 * If nArg is -1 that means to only return a match (non-zero) if p->nArg
 * is also -1.  In other words, we are searching for a function that
 * takes a variable number of arguments.
 *
 * If nArg is -2 that means that we are searching for any function
 * regardless of the number of arguments it uses, so return a positive
 * match score for any
 *
 * The returned value is always between 0 and 6, as follows:
 *
 * 0: Not a match.
 * 1: UTF8/16 conversion required and function takes any number of arguments.
 * 2: UTF16 byte order change required and function takes any number of args.
 * 3: encoding matches and function takes any number of arguments
 * 4: UTF8/16 conversion required - argument count matches exactly
 * 5: UTF16 byte order conversion required - argument count matches exactly
 * 6: Perfect match:  encoding and argument count match exactly.
 *
 * If nArg==(-2) then any function with a non-null xSFunc is
 * a perfect match and any function with xSFunc NULL is
 * a non-match.
 */
#define FUNC_PERFECT_MATCH 4	/* The score for a perfect match */
static int
matchQuality(FuncDef * p,	/* The function we are evaluating for match quality */
	     int nArg		/* Desired number of arguments.  (-1)==any */
    )
{
	int match;

	/* nArg of -2 is a special case */
	if (nArg == (-2))
		return (p->xSFunc == 0) ? 0 : FUNC_PERFECT_MATCH;

	/* Wrong number of arguments means "no match" */
	if (p->nArg != nArg && p->nArg >= 0)
		return 0;

	/* Give a better score to a function with a specific number of arguments
	 * than to function that accepts any number of arguments.
	 */
	if (p->nArg == nArg) {
		match = 4;
	} else {
		match = 1;
	}

	return match;
}

/*
 * Search a FuncDefHash for a function with the given name.  Return
 * a pointer to the matching FuncDef if found, or 0 if there is no match.
 */
static FuncDef *
functionSearch(int h,		/* Hash of the name */
	       const char *zFunc	/* Name of function */
    )
{
	FuncDef *p;
	for (p = sqlite3BuiltinFunctions.a[h]; p; p = p->u.pHash) {
		if (sqlite3StrICmp(p->zName, zFunc) == 0) {
			return p;
		}
	}
	return 0;
}

/*
 * Insert a new FuncDef into a FuncDefHash hash table.
 */
void
sqlite3InsertBuiltinFuncs(FuncDef * aDef,	/* List of global functions to be inserted */
			  int nDef	/* Length of the apDef[] list */
    )
{
	int i;
	for (i = 0; i < nDef; i++) {
		FuncDef *pOther;
		const char *zName = aDef[i].zName;
		int nName = sqlite3Strlen30(zName);
		int h =
		    (sqlite3UpperToLower[(u8) zName[0]] +
		     nName) % SQLITE_FUNC_HASH_SZ;
		pOther = functionSearch(h, zName);
		if (pOther) {
			assert(pOther != &aDef[i] && pOther->pNext != &aDef[i]);
			aDef[i].pNext = pOther->pNext;
			pOther->pNext = &aDef[i];
		} else {
			aDef[i].pNext = 0;
			aDef[i].u.pHash = sqlite3BuiltinFunctions.a[h];
			sqlite3BuiltinFunctions.a[h] = &aDef[i];
		}
	}
}

/*
 * Locate a user function given a name, a number of arguments and a flag
 * indicating whether the function prefers UTF-16 over UTF-8.  Return a
 * pointer to the FuncDef structure that defines that function, or return
 * NULL if the function does not exist.
 *
 * If the createFlag argument is true, then a new (blank) FuncDef
 * structure is created and liked into the "db" structure if a
 * no matching function previously existed.
 *
 * If nArg is -2, then the first valid function found is returned.  A
 * function is valid if xSFunc is non-zero.  The nArg==(-2)
 * case is used to see if zName is a valid function name for some number
 * of arguments.  If nArg is -2, then createFlag must be 0.
 *
 * If createFlag is false, then a function with the required name and
 * number of arguments may be returned even if the eTextRep flag does not
 * match that requested.
 */
FuncDef *
sqlite3FindFunction(sqlite3 * db,	/* An open database */
		    const char *zName,	/* Name of the function.  zero-terminated */
		    int nArg,	/* Number of arguments.  -1 means any number */
		    u8 createFlag	/* Create new entry if true and does not otherwise exist */
    )
{
	FuncDef *p;		/* Iterator variable */
	FuncDef *pBest = 0;	/* Best match found so far */
	int bestScore = 0;	/* Score of best match */
	int h;			/* Hash value */
	int nName;		/* Length of the name */
	struct session *user_session = current_session();

	assert(nArg >= (-2));
	assert(nArg >= (-1) || createFlag == 0);
	nName = sqlite3Strlen30(zName);

	/* First search for a match amongst the application-defined functions.
	 */
	p = (FuncDef *) sqlite3HashFind(&db->aFunc, zName);
	while (p) {
		int score = matchQuality(p, nArg);
		if (score > bestScore) {
			pBest = p;
			bestScore = score;
		}
		p = p->pNext;
	}

	/* If no match is found, search the built-in functions.
	 *
	 * If the SQLITE_PreferBuiltin flag is set, then search the built-in
	 * functions even if a prior app-defined function was found.  And give
	 * priority to built-in functions.
	 *
	 * Except, if createFlag is true, that means that we are trying to
	 * install a new function.  Whatever FuncDef structure is returned it will
	 * have fields overwritten with new information appropriate for the
	 * new function.  But the FuncDefs for built-in functions are read-only.
	 * So we must not search for built-ins when creating a new function.
	 */
	if (!createFlag &&
	    (pBest == 0
	     || (user_session->sql_flags & SQLITE_PreferBuiltin) != 0)) {
		bestScore = 0;
		h = (sqlite3UpperToLower[(u8) zName[0]] +
		     nName) % SQLITE_FUNC_HASH_SZ;
		p = functionSearch(h, zName);
		while (p) {
			int score = matchQuality(p, nArg);
			if (score > bestScore) {
				pBest = p;
				bestScore = score;
			}
			p = p->pNext;
		}
	}

	/* If the createFlag parameter is true and the search did not reveal an
	 * exact match for the name, number of arguments and encoding, then add a
	 * new entry to the hash table and return it.
	 */
	if (createFlag && bestScore < FUNC_PERFECT_MATCH &&
	    (pBest =
	     sqlite3DbMallocZero(db, sizeof(*pBest) + nName + 1)) != 0) {
		FuncDef *pOther;
		pBest->zName = (const char *)&pBest[1];
		pBest->nArg = (u16) nArg;
		pBest->funcFlags = 0;
		memcpy((char *)&pBest[1], zName, nName + 1);
		pOther =
		    (FuncDef *) sqlite3HashInsert(&db->aFunc, pBest->zName,
						  pBest);
		if (pOther == pBest) {
			sqlite3DbFree(db, pBest);
			sqlite3OomFault(db);
			return 0;
		} else {
			pBest->pNext = pOther;
		}
	}

	if (pBest && (pBest->xSFunc || createFlag)) {
		return pBest;
	}
	return 0;
}

/*
 * Free all resources held by the schema structure. The void* argument points
 * at a Schema struct. This function does not call sqlite3DbFree(db, ) on the
 * pointer itself, it just cleans up subsidiary resources (i.e. the contents
 * of the schema hash tables).
 *
 * The Schema.cache_size variable is not cleared.
 */
void
sqlite3SchemaClear(sqlite3 * db)
{
	assert(db->pSchema != NULL);

	Hash temp1;
	HashElem *pElem;
	Schema *pSchema = db->pSchema;

	temp1 = pSchema->tblHash;
	sqlite3HashInit(&pSchema->tblHash);
	for (pElem = sqliteHashFirst(&temp1); pElem;
	     pElem = sqliteHashNext(pElem)) {
		Table *pTab = sqliteHashData(pElem);
		sqlite3DeleteTable(0, pTab);
	}
	sqlite3HashClear(&temp1);

	db->pSchema = NULL;
}

/* Create a brand new schema. */
Schema *
sqlite3SchemaCreate(sqlite3 * db)
{
	struct Schema *p = (Schema *) sqlite3DbMallocZero(0, sizeof(Schema));
	if (p == NULL)
		sqlite3OomFault(db);
	else
		sqlite3HashInit(&p->tblHash);
	return p;
}
