s, err = pcall(box.error, 1, "err")
err.type
err.trace[1].file
err.trace[1].line
s, err = pcall(error, "oh no" )
err = err:unpack()
err.type
err.message
#err.trace

s, err = pcall(function() return a.b end)
err = err:unpack()
err.type
err.message
#err.trace

box.begin()
s, err = pcall(box.schema.user.create, "user" )
err = err:unpack()
err.type
err.message
#err.trace

errinj = box.error.injection
space = box.schema.space.create('tweedledum')
index = space:create_index('primary', { type = 'hash' })
errinj.set("ERRINJ_TESTING", true)
s, err = pcall(space.select, space, {222444})
errinj.set("ERRINJ_TESTING", false)
err= err:unpack()
err.type
err.message
#err.trace
space:drop()
