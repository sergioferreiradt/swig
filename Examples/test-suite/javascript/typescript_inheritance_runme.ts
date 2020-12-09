import { A, C, D } from './typescript-inheritance-types';

const typescript_inheritance = require("./typescript_inheritance");

const a: A = new typescript_inheritance.A();
a.internalAValue = 5;

const c: C = new typescript_inheritance.C();
c.intValue = 3;
c.floatValue = 2.45;
c.aValue = a;

const d: D = new typescript_inheritance.D();
d.intValue = 9;
d.floatValue = 3.1415;
d.aValue = a;
d.longValue = 33000;

process.exit(0);
