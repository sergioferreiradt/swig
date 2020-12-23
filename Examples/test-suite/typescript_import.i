%module typescript_import

%{
#include "typescript_d.h"
%}

%typemap(typescriptimports, noblock=1) SWIGTYPE {
import { * } from './typescript-import-d-types';

}


%inline %{
class C {
  public:
    int intValue;
    float floatValue;
    D dValue;
};
%}

