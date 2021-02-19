import { A, C } from './typescript-methods-types';

const typescript_interface = require("./typescript_methods");

const c: C = new typescript_interface.C();
const cValue: C = {
    intValue: 1,
    floatValue: 2.3,
};
c.intValue = 3;
c.floatValue = 2.45;
const s = c.sum(2,3);
c.funcWithStdString('Hello string function');
const a: A = c.funcWithClass(new typescript_interface.C());

process.exit(0);
