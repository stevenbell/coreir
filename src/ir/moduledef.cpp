
#include "moduledef.hpp"
#include "typegen.hpp"
#include <iterator>

using namespace std;

namespace CoreIR {



ModuleDef::ModuleDef(Module* module) : module(module), instancesIterFirst(nullptr), instancesIterLast(nullptr) {
  interface = new Interface(this,module->getContext()->Flip(module->getType()));
  cache = new SelCache();
}

ModuleDef::~ModuleDef() {
  //Delete interface, instances, cache
  delete interface;
  for(auto inst : instances) delete inst.second;
  delete cache;
}


void ModuleDef::print(void) {
  cout << "  Def:" << endl;
  cout << "    Instances:" << endl;
  for (auto inst : instances) {
    if (inst.second->isGen()) {
      cout << "      " << inst.first << " : " << inst.second->getGeneratorRef()->getName() << Args2Str(inst.second->getGenArgs()) << endl;
    }
    else {
      cout << "      " << inst.first << " : " << inst.second->getModuleRef()->getName() << endl;
    }
  }
  cout << "    Connections:\n";
  for (auto connection : connections) {
    cout << "      " << Connection2Str(connection) << endl;
  }
  cout << endl;
}

Context* ModuleDef::getContext() { return module->getContext(); }
const string& ModuleDef::getName() {return module->getName();}
Type* ModuleDef::getType() {return module->getType();}

ModuleDef* ModuleDef::copy() {
  Module* m = this->getModule();
  ModuleDef* def = m->newModuleDef();
  for (auto inst : this->getInstances()) {
    def->addInstance(inst.second);
  }

  for (auto con: this->getConnections()) {
    SelectPath a = con.first->getSelectPath();  
    SelectPath b = con.second->getSelectPath();
    def->connect(a,b);
  }
  return def;
}

//Can pass in either a single instance name
//Or pass in a '.' deleminated string
Wireable* ModuleDef::sel(string s) { 
  if (hasChar(s,'.')) {
    SelectPath path = splitString<SelectPath>(s,'.');
    return this->sel(path);
  }
  if (s=="self") return interface;
  else {
    ASSERT(instances.count(s),"Cannot find instance " + s);
    return instances[s]; 
  }
}

Wireable* ModuleDef::sel(SelectPath path) {
  Wireable* cur = this->sel(path[0]);
  for (auto it = std::next(path.begin()); it != path.end(); ++it) {
    cur = cur->sel(*it);
  }
  return cur;
}

void ModuleDef::appendInstanceToIter(Instance* instance) {
    if (instancesIterFirst == nullptr) {
        assert(this->instancesIterLast == nullptr);
        // Sets `instance` to the current first and last pointer of the
        // iterator and sets the prev and next pointers to be null
        this->instancesIterFirst = instance;
        this->instancesIterLast = instance;
        this->instancesIterNextMap[instance] = nullptr;
        this->instancesIterPrevMap[instance] = nullptr;
    } else {
        assert(this->instancesIterLast != nullptr);
        Instance* currLast = this->instancesIterLast;
        // Updates the current last instance's next to point to the new instance,
        assert(this->instancesIterNextMap[currLast] == nullptr);  // current last shouldn't have a next
        this->instancesIterNextMap[currLast] = instance;
        // Sets the new instance's prev to point the current last instance,
        this->instancesIterPrevMap[instance] = currLast;
        // Sets the new instance's next to be null
        this->instancesIterNextMap[instance] = nullptr;
        // Sets the current last instance to be the new instance
        this->instancesIterLast = instance;
    }
}

void ModuleDef::removeInstanceFromIter(Instance* instance) {
    assert(this->instancesIterNextMap.count(instance) == 1);
    assert(this->instancesIterPrevMap.count(instance) == 1);
    // Because we set prev and next to be nullptr for the first and last, we
    // can assume the prev and next exist or are null
    Instance *next = this->instancesIterNextMap[instance];
    Instance *prev = this->instancesIterPrevMap[instance];
    // Update pointers to skip instance
    this->instancesIterNextMap[prev] = next;
    this->instancesIterPrevMap[next] = prev;
    if (instance == this->instancesIterLast) {
        // The new last is this instance's prev
        this->instancesIterLast = prev;
    } else if (instance == this->instancesIterFirst) {
        // The new first is the this instance's next
        this->instancesIterFirst = next;
    }
}

Instance* ModuleDef::getInstancesIterNext(Instance* instance) {
    ASSERT(instance, "Cannot get next of IterEnd");
    ASSERT(this->instancesIterNextMap.count(instance) == 1, "DEBUG ME: instance not in iter") // TOOD: Should be an error?
    return this->instancesIterNextMap[instance];
}


Instance* ModuleDef::addInstance(string instname,Generator* gen, Args genargs,Args config) {
  assert(instances.count(instname)==0);

  Instance* inst = new Instance(this,instname,gen,genargs,config);
  instances[instname] = inst;

  appendInstanceToIter(inst);

  return inst;
}

Instance* ModuleDef::addInstance(string instname,Module* m,Args config) {
  Instance* inst = new Instance(this,instname,m,config);
  instances[instname] = inst;
  
  appendInstanceToIter(inst);
  
  return inst;
}

Instance* ModuleDef::addInstance(string instname,string iref,Args genOrConfigargs, Args configargs) {
  vector<string> split = splitRef(iref);
  Instantiable* ref = this->getContext()->getInstantiable(iref);
  if (auto g = dyn_cast<Generator>(ref)) {
    return this->addInstance(instname,g,genOrConfigargs,configargs);
  }
  else {
    auto m = cast<Module>(ref);
    return this->addInstance(instname,m,genOrConfigargs);
  }
}

Instance* ModuleDef::addInstance(Instance* i,string iname) {
  if (iname=="") {
    iname = i->getInstname();
  }
  if( i->isGen()) 
    return addInstance(iname,i->getGeneratorRef(),i->getGenArgs(),i->getConfigArgs());
  else 
    return addInstance(iname,i->getModuleRef(),i->getConfigArgs());
}

void ModuleDef::connect(Wireable* a, Wireable* b) {
  //Make sure you are connecting within the same context
  Context* c = getContext();
  if (a->getContainer()!=this || b->getContainer() != this) {
    Error e;
    e.message("connections can only occur within the same module");
    e.message("  This ModuleDef: " + module->getName());
    e.message("  ModuleDef of " + a->toString() + ": " + a->getContainer()->getName());
    e.message("  ModuleDef of " + b->toString() + ": " + b->getContainer()->getName());
    c->error(e);
    return;
  }

  // TODO should I type check here at all?
  //checkWiring(a,b);
  
  Connection connect = connectionCtor(a,b);
  if (!connections.count(connect)) {
    
    //Update 'a' and 'b'
    a->addConnectedWireable(b);
    b->addConnectedWireable(a);
    connections.insert(connect);
  }
  else {
    cout << "ALREADY ADDED CONNECTION!" << endl;
  }
}

void ModuleDef::connect(SelectPath pathA, SelectPath pathB) {
  this->connect(this->sel(pathA),this->sel(pathB));
}

void ModuleDef::connect(string pathA, string pathB) {
  this->connect(this->sel(pathA),this->sel(pathB));
}
void ModuleDef::connect(std::initializer_list<const char*> pA, std::initializer_list<const char*> pB) {
  connect(SelectPath(pA.begin(),pA.end()),SelectPath(pB.begin(),pB.end()));
}
void ModuleDef::connect(std::initializer_list<std::string> pA, std::initializer_list<string> pB) {
  connect(SelectPath(pA.begin(),pA.end()),SelectPath(pB.begin(),pB.end()));
}
bool ModuleDef::hasConnection(Wireable* a, Wireable* b) {
  Connection con = connectionCtor(a,b);
  return connections.count(con) > 0;
}

Connection ModuleDef::getConnection(Wireable* a, Wireable* b) {
  Connection con = connectionCtor(a,b);
  ASSERT(connections.count(con)>0,"Could not find connection!");
  
  return *connections.find(con);
}

//This will remove all connections from a specific wireable
void ModuleDef::disconnect(Wireable* w) {
  for (auto wc : w->getConnectedWireables()) {
    this->disconnect(w,wc);
  }
}

void ModuleDef::disconnect(Wireable* a, Wireable* b) {
  Connection connect = connectionCtor(a,b);
  this->disconnect(connect);
}
void ModuleDef::disconnect(Connection con) {
  ASSERT(connections.count(con),"Cannot delete connection that is not connected!");
  
  //remove references
  con.first->removeConnectedWireable(con.second);
  con.second->removeConnectedWireable(con.first);

  //Delete connection from list
  connections.erase(con);
}

void disconnectAllWireables(ModuleDef* m, Wireable* w) {
  for (auto sels : w->getSelects()) {
    disconnectAllWireables(m,sels.second);
  }
  m->disconnect(w);
}
void ModuleDef::removeInstance(Instance* inst) {
  removeInstance(inst->getInstname());
}

void ModuleDef::removeInstance(string iname) {
  //First verify that instance exists
  ASSERT(instances.count(iname), "Instance " + iname + " does not exist");
  Instance* inst = instances.at(iname);
  
  //First remove all the connections from this instance
  disconnectAllWireables(this,inst);

  //Now remove this instance
  instances.erase(iname);
  
  removeInstanceFromIter(inst);

}

} //coreir namespace
