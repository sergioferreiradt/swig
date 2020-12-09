import { A, B, C } from './typescript-remapbasetype-types';

const typescript_remapbasetype = require("./typescript_remapbasetype");

const a: A = new typescript_remapbasetype.A();
a.internalAValue = 5;

const c: C = new typescript_remapbasetype.C();
c.intBValue = 2;
c.intValue = 3;
c.floatValue = 2.45;
c.aValue = a;

process.exit(0);
