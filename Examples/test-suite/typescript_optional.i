%module typescript_optional


%inline %{
template <class T>
class OptionalValue
{
  public:
    T v;
};
%}

%typemap (typescriptoptional, optional="1") OptionalValue<nm::D> "nm::D";
%typemap (typescriptoptional, optional="1") OptionalValue<int> "int";
%typemap (typescriptoptional, optional="0") OptionalValue<long> "long";
%typemap (typescriptoptional, optional="1") OptionalValue<unsigned> "unsigned";
%typemap (typescriptoptional, optional="1") OptionalValue<nm::Foo> "nm::Foo";

%template(OptionalD) OptionalValue<nm::D>;
%template(OptionalInt) OptionalValue<int>;
%template(OptionalLong) OptionalValue<long>;
%template(OptionalUnsigned) OptionalValue<unsigned>;


%inline %{

namespace nm {

enum Foo {
  a,b,c
};

class D {
  public:
    double doubleValue;
};


class A {
  public:
     char a;
     int b;
     double c;
};

class C {
  public:
    int intValue;
    float floatValue;
    OptionalValue<nm::D> optionalDProperty;
    A aProperty;
    OptionalValue<int> optionalIntValue;
    OptionalValue<long> notMarkedOptionalLongValue;
    OptionalValue<unsigned> optionalUnsignedValue;
    OptionalValue<Foo> optionalEnumValue;
};
}

%}

