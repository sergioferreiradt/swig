%module typescript_inheritance

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
class D: public C {
  public:
    long longValue;
};
%}

