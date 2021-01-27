%module typescript_import

%{
#include "typescript_d.h"
%}

%pragma(javascript) tsimports=%{
import { X } from './typescript-import-d-types';
%}

%pragma(javascript) tsimports=%{
import { D } from './typescript-import-d-types';
%}

%inline %{
class C {
  public:
    int intValue;
    float floatValue;
    D dValue;
};
%}


