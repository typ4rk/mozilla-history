/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */

var gTestfile = 'regress-469625-02.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 469625;
var summary = 'group assignment with rhs containing holes';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = 'y';

  Array.prototype[1] = 'y';
  var [x, y, z] = ['x', , 'z'];

  actual = y;
 
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
