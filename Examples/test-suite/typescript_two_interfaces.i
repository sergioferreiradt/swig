%module typescript_two_interfaces

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

