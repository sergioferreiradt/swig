%module typescript_interface

%inline %{
class A {
  public:
    int internalAValue;
};
class C {
  public:
    int intValue;
    float floatValue;
    A aValue;
};
%}

