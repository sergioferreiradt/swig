import { C } from './typescript-optional-types';

const typescript_interface = require("./typescript_optional");

const c: C = {
    intValue: 1,
    floatValue: 2.3,
    aProperty : {
        a: 'c',
        b: 1,
        c: 2.2
    },
    notMarkedOptionalLongValue : { v: 2.1 },
    optionalUnsignedValue: { v: 2 }

};

process.exit(0);
