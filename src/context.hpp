#ifndef CONTEXT_HPP_
#define CONTEXT_HPP_

#include "namespace.hpp"
#include "typecache.hpp"
#include "types.hpp"
#include "error.hpp"
#include "common.hpp"

#include <string>
#include <unordered_set>
#include <vector>

using namespace std;

namespace CoreIR {

class Context {
  Namespace* global;
  map<string,Namespace*> libs;
  
  uint maxErrors;
  vector<Error> errors;
 
  //Memory management
  TypeCache* cache;
  
  vector<Arg*> argList;
  vector<Args*> argsList;
  vector<RecordParams*> recordParamsList;
  vector<Params*> paramsList;
  vector<Instance**> instanceArrays;
  vector<Connection*> connectionArrays;
  vector<Connection**> connectionPtrArrays;
  vector<Wireable**> wireableArrays;
  vector<const char**> constStringArrays;

  public :
    Context();
    ~Context();
    Namespace* getGlobal() {return global;}
    
    //Error functions
    void error(Error e) { 
      errors.push_back(e);
      if (e.isfatal || errors.size() >= maxErrors) die();
    }
    bool haserror() { return errors.size()>0; }
    void checkerrors() { if (haserror()) die(); }
    void die();
    void printerrors() { 
      for (auto err : errors) cout << "ERROR: " << err.toString() << endl << endl;
    }
    
    bool linkLib(Namespace* defns, Namespace* declns);
    
    Namespace* newNamespace(string name);
    bool hasNamespace(string name) { return libs.count(name) > 0; }
    Namespace* getNamespace(string s);
    map<string,Namespace*> getNamespaces() {return libs;}

    Type* Any();
    Type* BitIn();
    Type* Bit();
    Type* Array(uint n, Type* t);
    Type* Record(RecordParams rp);
    Type* Named(string ns, string name);
    Type* Named(string ns, string name, Args args);
    Type* Flip(Type* t);
    Type* In(Type* t);
    Type* Out(Type* t);

    TypeGen* getTypeGen(string ns, string name);

    RecordParams* newRecordParams();
    Params* newParams();
    Args* newArgs();
    
    Arg* int2Arg(int i);
    Arg* str2Arg(string s);
    Arg* type2Arg(Type* t);

    Instance** newInstanceArray(int size);
    Connection* newConnectionArray(int size);
    Connection** newConnectionPtrArray(int size);
    Wireable** newWireableArray(int size);
    const char** newConstStringArray(int size);

};

Module* loadModule(Context* c, string filename, bool* err);
void saveModule(Module* c, string filename, bool* err);

Context* newContext();
void deleteContext(Context* m);

} //CoreIR namespace

#endif //CONTEXT_HPP_
