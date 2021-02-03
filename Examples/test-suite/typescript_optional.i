%module typescript_optional

%typemap (typescriptoptional) OptionalD "";

%inline %{

class OptionalD {
  public:
    double doubleValue;
};

class C {
  public:
    int intValue;
    float floatValue;
    OptionalD optionalProperty;
};
%}

