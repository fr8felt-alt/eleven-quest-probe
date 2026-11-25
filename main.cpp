// Eleven Table Tennis / Quest 3 - three visual training targets (experimental)
// Native Scotland2 mod: runs at late_load, when Unity IL2CPP has initialized.
// Does not interact with ball launcher, scoring, tracking, or physics.
// IL2CPP runtime_invoke: value-type params point to value storage (e.g. &Vec3),
 // reference-type params point directly to Il2CppObject (not &objPtr).
#include <android/log.h>
#include <dlfcn.h>
#include <cstdint>
#include <cstring>
#include <string>
#include "scotland2/shared/modloader.h"

static constexpr const char* TAG = "ElevenTargets";
static CModInfo info{"eleven-quest-targets", "0.2.1", 0};
#define MOD_EXTERN_FUNC extern "C" __attribute__((visibility("default")))
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

namespace {
struct Vec3 { float x, y, z; };
struct Color { float r, g, b, a; };
struct Quaternion { float x, y, z, w; };
struct Obj { void* klass; void* monitor; };
struct Array { Obj object; void* bounds; uintptr_t max_length; void* items[1]; };
using DomainGet=void*(*)();
using GetAssemblies=void**(*)(void*, size_t*);
using AssemblyGetImage=void*(*)(void*);
using ImageGetName=const char*(*)(void*);
using ClassFromName=void*(*)(void*, const char*,const char*);
using ClassGetMethod=void*(*)(void*,const char*,int);
using ClassGetType=const void*(*)(void*);
using TypeGetObject=void*(*)(const void*);
using RuntimeInvoke=void*(*)(const void*,void*,void**,void**);
using ObjUnbox=void*(*)(void*);
using StringNew=void*(*)(const char*);

struct Api {
  void* handle{};
  DomainGet domain_get{};
  GetAssemblies get_assemblies{};
  AssemblyGetImage assembly_get_image{};
  ImageGetName image_get_name{};
  ClassFromName class_from_name{};
  ClassGetMethod class_get_method{};
  ClassGetType class_get_type{};
  TypeGetObject type_get_object{};
  RuntimeInvoke invoke{};
  ObjUnbox unbox{};
  StringNew str_new{};
  template<class T> bool symbol(T& p, const char* name) {
    p=reinterpret_cast<T>(dlsym(handle,name));
    if (!p) LOGE("Faltando IL2CPP: %s",name);
    return p!=nullptr;
  }
  bool init() {
    handle=dlopen("libil2cpp.so",RTLD_NOW|RTLD_NOLOAD);
    if (!handle) { LOGE("libil2cpp.so indisponivel: %s", dlerror()); return false; }
    return symbol(domain_get,"il2cpp_domain_get") &&
      symbol(get_assemblies,"il2cpp_domain_get_assemblies") &&
      symbol(assembly_get_image,"il2cpp_assembly_get_image") &&
      symbol(image_get_name,"il2cpp_image_get_name") &&
      symbol(class_from_name,"il2cpp_class_from_name") &&
      symbol(class_get_method,"il2cpp_class_get_method_from_name") &&
      symbol(class_get_type,"il2cpp_class_get_type") &&
      symbol(type_get_object,"il2cpp_type_get_object") &&
      symbol(invoke,"il2cpp_runtime_invoke") &&
      symbol(unbox,"il2cpp_object_unbox") &&
      symbol(str_new,"il2cpp_string_new");
  }
  void* klass(const char* imageName,const char* ns,const char* name) const {
    size_t count=0; void* dom=domain_get();
    if (!dom) return nullptr;
    auto assemblies=get_assemblies(dom,&count);
    for (size_t i=0;assemblies && i<count;++i) {
      auto image=assembly_get_image(assemblies[i]);
      if (!image) continue;
      auto actual=image_get_name(image);
      // Image name normally includes .dll
      if (actual && (std::strcmp(actual,imageName)==0))
        return class_from_name(image,ns,name);
    }
    return nullptr;
  }
  void* method(void* cl,const char* name,int argc) const {
    return cl?class_get_method(cl,name,argc):nullptr;
  }
  void* call(void* m,void* instance,void** args=nullptr) const {
    if (!m) return nullptr;
    void* ex=nullptr;
    void* result=invoke(m,instance,args,&ex);
    if (ex) { LOGE("Excecao gerenciada ao invocar metodo Unity/IL2CPP"); return nullptr; }
    return result;
  }
};

static void* call1(Api& api, void* method, void* owner,void* a) {
  void* args[]={a}; return api.call(method,owner,args);
}
static bool write1(Api& api,void* method,void* owner,void* arg) {
  if (!method || !owner) return false;
  void* ex=nullptr; void* args[]={arg}; api.invoke(method,owner,args,&ex);
  if (ex) { LOGE("Excecao Unity ajustando componente");return false; }
  return true;
}
static void* getTableSurface(Api& api,void* goClass,void* objectClass) {
  // PC investigation: TableSurfaceSpace_forTargets exists but may be disabled.
  auto resources=api.klass("UnityEngine.CoreModule.dll","UnityEngine","Resources");
  auto getAll=api.method(resources,"FindObjectsOfTypeAll",1);
  auto getName=api.method(objectClass,"get_name",0);
  if (!getAll||!getName) return nullptr;
  void* goType=api.type_get_object(api.class_get_type(goClass));
  LOGI("Procurando o objeto da superficie da mesa (Resources.FindObjectsOfTypeAll)...");
  auto arr=reinterpret_cast<Array*>(call1(api,getAll,nullptr,goType));
  LOGI("FindObjectsOfTypeAll retornou: %p", static_cast<void*>(arr));
  if (!arr || arr->max_length>100000) return nullptr;
  for (uintptr_t i=0;i<arr->max_length;++i) {
    void* go=arr->items[i]; if (!go) continue;
    void* name=api.call(getName,go);
    if (!name) continue;
    // Il2CppString: Obj header + int32 length + UTF16 buffer
    const auto* length=reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(name)+sizeof(Obj));
    static constexpr char kTarget[]="TableSurfaceSpace_forTargets";
    if (*length != static_cast<int32_t>(sizeof(kTarget)-1)) continue;
    const auto* s=reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(name)+sizeof(Obj)+4);
    bool eq=true;
    for (size_t j=0;j<sizeof(kTarget);++j) {
      if (j==sizeof(kTarget)-1) break;
      if (s[j] != static_cast<uint16_t>(kTarget[j])) {eq=false;break;}
    }
    if(eq) return go;
  }
  return nullptr;
}

static void createTargets() {
  Api a;
  if (!a.init()) return;
  LOGI("API IL2CPP inicializada; procurando classes Unity...");
  auto gameObject=a.klass("UnityEngine.CoreModule.dll","UnityEngine","GameObject");
  auto transform=a.klass("UnityEngine.CoreModule.dll","UnityEngine","Transform");
  auto object=a.klass("UnityEngine.CoreModule.dll","UnityEngine","Object");
  auto component=a.klass("UnityEngine.CoreModule.dll","UnityEngine","Component");
  auto renderer=a.klass("UnityEngine.CoreModule.dll","UnityEngine","Renderer");
  auto material=a.klass("UnityEngine.CoreModule.dll","UnityEngine","Material");
  auto collider=a.klass("UnityEngine.PhysicsModule.dll","UnityEngine","Collider");
  if (!gameObject||!transform||!object||!component||!renderer||!material) {
    LOGE("Classes basicas Unity nao disponiveis: alvos nao criados");return;
  }
  LOGI("Classes Unity resolvidas. Buscando metodos...");
  auto createPrimitive=a.method(gameObject,"CreatePrimitive",1);
  auto getTransform=a.method(gameObject,"get_transform",0);
  auto setName=a.method(object,"set_name",1);
  auto setPosition=a.method(transform,"set_position",1);
  auto setScale=a.method(transform,"set_localScale",1);
  auto getRotation=a.method(transform,"get_rotation",0);
  auto setRotation=a.method(transform,"set_rotation",1);
  auto transformPoint=a.method(transform,"TransformPoint",1);
  auto getComponent=a.method(gameObject,"GetComponent",1);
  auto getMaterial=a.method(renderer,"get_material",0);
  auto setColor=a.method(material,"set_color",1);
  auto setColliderEnabled=a.method(collider,"set_enabled",1);
  auto destroy=a.method(object,"Destroy",1);
  auto dontDestroy=a.method(object,"DontDestroyOnLoad",1);
  if(!createPrimitive||!getTransform||!setPosition||!setScale||
     !getComponent||!collider||!setColliderEnabled||!renderer||!getMaterial||!setColor) {
    LOGE("Componentes de desenho ou seguranca do collider nao disponiveis");return;
  }
  LOGI("Metodos Unity resolvidos. Procurando superficie...");
  void* surfaceGo=getTableSurface(a,gameObject,object);
  void* surfaceTransform=surfaceGo ? a.call(getTransform,surfaceGo) : nullptr;
  LOGI("TableSurfaceSpace_forTargets: %s", surfaceTransform?"encontrado":"nao encontrado; usando posicao aproximada");
  Quaternion q{0,0,0,1};
  if(surfaceTransform&&getRotation) {
    auto boxed=a.call(getRotation,surfaceTransform);
    if(boxed) std::memcpy(&q,a.unbox(boxed),sizeof(q));
  }
  constexpr float xs[]={-0.46f,0.0f,0.46f};
  constexpr const char* names[]={"Esquerda","Centro","Direita"};
  int created=0;
  for (int i=0;i<3;++i) {
    LOGI("Criando alvo %d/3", i+1);
    int primitiveCylinder=2; // UnityEngine.PrimitiveType.Cylinder
    void* disk=call1(a,createPrimitive,nullptr,&primitiveCylinder);
    if(!disk){LOGE("CreatePrimitive falhou alvo %d",i);continue;}
    if(setName){
      auto n=std::string("Eleven_Alvo_Amarelo_")+names[i];
      void* name=a.str_new(n.c_str());
      write1(a,setName,disk,name);
    }
    // CreatePrimitive includes a physics collider. Disable it *before* placing it on the table.
    void* colliderType=a.type_get_object(a.class_get_type(collider));
    auto hitbox=call1(a,getComponent,disk,colliderType);
    bool disabled=false;
    if (!hitbox || !write1(a,setColliderEnabled,hitbox,&disabled)) {
      LOGE("Nao consegui desativar collider do alvo %d; descartando o objeto", i);
      if(destroy){void* arg=disk;call1(a,destroy,nullptr,arg);}
      continue;
    }
    void* diskTransform=a.call(getTransform,disk);
    if(!diskTransform)continue;
    // 18 cm diameter, very thin; positioned a few mm above the table.
    Vec3 local{xs[i],0.008f,0.86f};
    Vec3 world{xs[i],0.767f,0.86f};
    if(surfaceTransform && transformPoint) {
      void* boxed=call1(a,transformPoint,surfaceTransform,&local);
      if(boxed) std::memcpy(&world,a.unbox(boxed),sizeof(world));
    }
    write1(a,setPosition,diskTransform,&world);
    Vec3 scale{0.18f,0.001f,0.18f};
    write1(a,setScale,diskTransform,&scale);
    if(surfaceTransform&&setRotation) write1(a,setRotation,diskTransform,&q);
    if(getComponent && renderer && getMaterial && setColor){
      void* type=a.type_get_object(a.class_get_type(renderer));
      auto render=call1(a,getComponent,disk,type);
      if(render){
        auto mat=a.call(getMaterial,render);
        if(mat){Color yellow{1.0f,0.83f,0.05f,1.0f};write1(a,setColor,mat,&yellow);}
      }
    }
    if(dontDestroy){void* arg=disk;call1(a,dontDestroy,nullptr,arg);}
    ++created;
  }
  LOGI("Alvos criados: %d/3",created);
}
} // namespace

MOD_EXTERN_FUNC void setup(CModInfo* output) noexcept {
  *output=info;
  LOGI("setup: modulo visual v0.2.1 reconhecido");
}
MOD_EXTERN_FUNC void late_load() noexcept {
  LOGI("late_load v0.2.1: tentando criar 3 alvos amarelos no Unity");
  createTargets();
}
