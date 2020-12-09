import { A, C } from './typescript-multiple-inheritance-types';

const typescript_multiple_inheritance = require("./typescript_multiple_inheritance");

const c: C = new typescript_multiple_inheritance.C();
c.intAValue = 2;
c.intValue = 3;
c.floatValue = 2.45;

process.exit(0);
