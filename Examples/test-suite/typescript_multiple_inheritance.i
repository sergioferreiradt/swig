%module typescript_multiple_inheritance

%ignore B;

%inline %{
class A {
  public:
    int intAValue;
};
class B {
  public:
    int intBValue;
};
class C : public A, public B {
  public:
    int intValue;
    float floatValue;
};
%}

