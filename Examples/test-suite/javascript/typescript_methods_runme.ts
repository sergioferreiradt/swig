import { C } from './typescript-methods-types';

const typescript_interface = require("./typescript_methods");

const c: C = new typescript_interface.C();
c.intValue = 3;
c.floatValue = 2.45;
const s = c.sum(2,3);

process.exit(0);
