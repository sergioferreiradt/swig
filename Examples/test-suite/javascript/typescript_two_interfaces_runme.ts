import { A, C } from './typescript-two-interfaces-types';

const typescript_two_interfaces = require("./typescript_two_interfaces");

const a: A = new typescript_two_interfaces.A();
a.internalAValue = 5;

const c: C = new typescript_two_interfaces.C();
c.intValue = 3;
c.floatValue = 2.45;
c.aValue = a;

process.exit(0);
