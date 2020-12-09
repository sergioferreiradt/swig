import { C } from './typescript-interface-types';

const typescript_interface = require("./typescript_interface");

const c: C = new typescript_interface.C();
c.intValue = 3;
c.floatValue = 2.45;

process.exit(0);
