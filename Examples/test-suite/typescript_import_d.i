%module typescript_import_d

%inline %{
class D {
  public:
    int intValue;
    float floatValue;
};
%}

