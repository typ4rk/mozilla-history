// |jit-test| debug
// The same object gets the same Debug.Object wrapper at different times, if the difference would be observable.

var N = HOTLOOP + 4;

var g = newGlobal('new-compartment');
var dbg = Debug(g);
var wrappers = [];

dbg.hooks = {debuggerHandler: function (frame) { wrappers.push(frame.arguments[0]); }};
g.eval("var originals = []; function f(x) { originals.push(x); debugger; }");
for (var i = 0; i < N; i++)
    g.eval("f({});");
assertEq(wrappers.length, N);

gc();

dbg.hooks = {debuggerHandler: function (frame) { assertEq(frame.arguments[0], wrappers.pop()); }};
g.eval("function h(x) { debugger; }");
for (var i = 0; i < N; i++)
    g.eval("h(originals.pop());");
assertEq(wrappers.length, 0);
