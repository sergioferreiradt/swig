%module typescript_import

%{
#include "typescript_d.h"
%}

%inline %{
class C {
  public:
    int intValue;
    float floatValue;
    D dValue;
};
%}

%pragma(javascript) tsimports=%{
import { D } from './typescript-import-d-types';
%}

