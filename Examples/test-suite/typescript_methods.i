%module typescript_methods

%inline %{

class A {
  std::string strValue;
};

class C {
  public:
    int intValue;
    float floatValue;
    int sum(int a,int b) { return a+b;}
    void funcWithStdString(std::string str) { }
    A funcWithClass(C cVar) { }
};
%}

