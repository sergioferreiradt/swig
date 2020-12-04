%module typescript_interface

%inline %{
class C {
  public:
    int intValue;
    float floatValue;
    
};
%}

