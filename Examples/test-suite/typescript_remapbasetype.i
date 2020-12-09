%module typescript_remapbasetype

%typemap(typescriptbase) C "B"

%inline %{
class A {
  public:
    int internalAValue;
};
class B {
  public:
    int intBValue;
};
class C : public A {
  public:
    int intValue;
    float floatValue;
    A aValue;
};
%}

