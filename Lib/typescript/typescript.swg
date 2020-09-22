/* -----------------------------------------------------------------------------
 * typescript.swg
 *
 * TypeScript typemaps
 * ----------------------------------------------------------------------------- */


%include <typemaps/swigmacros.swg>


%include <javascripttypemaps.swg>


%include <javacriptruntime.swg>


%include <javascripthelpers.swg>

%include <javascriptkw.swg>

%include <javascriptcode.swg>

%include <javascriptinit.swg>


%typemap(typescripttype) bool,               const bool &               "boolean"
%typemap(typescripttype) char,               const char &               "string"
%typemap(typescripttype) signed char,        const signed char &        "string"
%typemap(typescripttype) unsigned char,      const unsigned char &      "number"
%typemap(typescripttype) short,              const short &              "number"
%typemap(typescripttype) unsigned short,     const unsigned short &     "number"
%typemap(typescripttype) int,                const int &                "number"
%typemap(typescripttype) unsigned int,       const unsigned int &       "number"
%typemap(typescripttype) long,               const long &               "number"
%typemap(typescripttype) unsigned long,      const unsigned long &      "number"
%typemap(typescripttype) long long,          const long long &          "number"
%typemap(typescripttype) unsigned long long, const unsigned long long & "number"
%typemap(typescripttype) float,              const float &              "number"
%typemap(typescripttype) double,             const double &             "number"
%typemap(typescripttype) void                                           "number"