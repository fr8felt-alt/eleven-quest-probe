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
static CModInfo info{"eleven-quest-targets", "0.2.4", 0};
#define MOD_EXTERN_FUNC extern "C" __attribute__((visibility("default")))
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

namespace {
struct Vec3 { float x, y, z; };
struct Color { float r, g, b, a; };
struct Quaternion { float x, y, z, w; };
// Rotate a physical displacement, without applying Transform.localScale.
// This matches the PC mod's unscaled holder: position=table.position,
// rotation=table.rotation, localScale=Vector3.one.
static Vec3 rotateByQuaternion(const Quaternion& q, const Vec3& v) {
  const Vec3 t{
    2.0f * (q.y * v.z - q.z * v.y),
    2.0f * (q.z * v.x - q.x * v.z),
    2.0f * (q.x * v.y - q.y * v.x)
  };
  return Vec3{
    v.x + q.w * t.x + (q.y * t.z - q.z * t.y),
    v.y + q.w * t.y + (q.z * t.x - q.x * t.z),
    v.z + q.w * t.z + (q.x * t.y - q.y * t.x)
  };
}
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
  auto getPosition=a.method(transform,"get_position",0);
  auto getParent=a.method(transform,"get_parent",0);
  auto setParent=a.method(transform,"SetParent",2);
  auto getComponent=a.method(gameObject,"GetComponent",1);
  auto getMaterial=a.method(renderer,"get_material",0);
  auto setColor=a.method(material,"set_color",1);
  auto setColliderEnabled=a.method(collider,"set_enabled",1);
  auto destroy=a.method(object,"Destroy",1);
  auto dontDestroy=a.method(object,"DontDestroyOnLoad",1);
  if(!createPrimitive||!getTransform||!getPosition||!getRotation||!setPosition||!setScale||
     !getComponent||!collider||!setColliderEnabled||!renderer||!getMaterial||!setColor) {
    LOGE("Componentes de desenho ou seguranca do collider nao disponiveis");return;
  }
  LOGI("Metodos Unity resolvidos. Procurando superficie...");
  void* surfaceGo=getTableSurface(a,gameObject,object);
  void* surfaceTransform=surfaceGo ? a.call(getTransform,surfaceGo) : nullptr;
  LOGI("TableSurfaceSpace_forTargets: %s", surfaceTransform?"encontrado":"nao encontrado; usando posicao aproximada");
  // Attach under GameTable, not under the potentially inactive TableSurfaceSpace.
  // That way targets follow subsequent table movements/recentering.
  void* tableParent=(surfaceTransform && getParent) ? a.call(getParent,surfaceTransform) : nullptr;
  LOGI("Transform pai de TableSurfaceSpace: %p; SetParent: %s",
       tableParent, setParent?"disponivel":"indisponivel");
  Quaternion q{0,0,0,1};
  Vec3 framePos{0.0f,0.7586f,0.0f};
  if (surfaceTransform) {
    auto boxedRotation=a.call(getRotation,surfaceTransform);
    if (boxedRotation) std::memcpy(&q,a.unbox(boxedRotation),sizeof(q));
    auto boxedPosition=a.call(getPosition,surfaceTransform);
    if (boxedPosition) std::memcpy(&framePos,a.unbox(boxedPosition),sizeof(framePos));
  }
  LOGI("Mesa frame pos=(%.3f,%.3f,%.3f), rot=(%.3f,%.3f,%.3f,%.3f)",
       framePos.x,framePos.y,framePos.z,q.x,q.y,q.z,q.w);
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
    // IMPORTANT: coordinates are physical meters, not scaled local coordinates.
    // Unity's TransformPoint multiplies by table scale (1.5258 x 2.7453
    // on the PC prototype), putting the targets beyond the table.
    // Calibration based on casting screenshot: v0.2.3 (+0.7586 m) floats above
    // tabletop, and the whole row appears left of the opponent-side center.
    // Both values below are ESTIMATES; calibrate from another screenshot.
    // Keep them explicit to make later iterations a two-number change.
    constexpr float kRightShiftMeters = 0.40f;
    constexpr float kTableTopOffsetY = 0.36f;
    Vec3 local{xs[i] + kRightShiftMeters,kTableTopOffsetY + 0.008f,0.86f};
    const Vec3 rotated=rotateByQuaternion(q,local);
    Vec3 world{framePos.x+rotated.x,
               framePos.y+rotated.y,
               framePos.z+rotated.z};
    LOGI("Alvo %d: posicao em metros=(%.3f,%.3f,%.3f)",
         i+1,world.x,world.y,world.z);
    if (!write1(a,setPosition,diskTransform,&world)) {
      LOGE("Falha ao posicionar alvo %d",i+1); continue;
    }
    Vec3 scale{0.18f,0.001f,0.18f};
    write1(a,setScale,diskTransform,&scale);
    if(surfaceTransform&&setRotation) write1(a,setRotation,diskTransform,&q);
    if(getComponent && renderer && getMaterial && setColor){
      void* type=a.type_get_object(a.class_get_type(renderer));
      void* render=call1(a,getComponent,disk,type);
      if (render) {
        auto mat=a.call(getMaterial,render);
        if (mat) {
          Color yellow{1.0f,0.83f,0.05f,1.0f};
          if (!write1(a,setColor,mat,&yellow))
            LOGE("Nao consegui definir cor do alvo %d",i+1);
        } else {
          LOGE("Sem material para o alvo %d",i+1);
        }
      } else {
        LOGE("Sem Renderer para o alvo %d",i+1);
      }
    }
    // Follow the GameTable when the user repositions it in VR.
    // worldPositionStays=true keeps the positions calibrated above.
    bool parented=false;
    if(tableParent && setParent){
      bool keepWorld=true;
      void* parentArgs[]={tableParent,&keepWorld};
      void* ex=nullptr;
      a.invoke(setParent,diskTransform,parentArgs,&ex);
      parented=(ex==nullptr);
      LOGI("Alvo %d: associado ao pai da mesa = %s", i+1,
           parented?"sim":"nao");
    }
    // DontDestroyOnLoad only applies to root objects, not children.
    if(!parented && dontDestroy){call1(a,dontDestroy,nullptr,disk);}
    ++created;
  }
  LOGI("Alvos criados: %d/3; verificacao visual ainda necessaria",created);
}
} // namespace

MOD_EXTERN_FUNC void setup(CModInfo* output) noexcept {
  *output=info;
  LOGI("setup: modulo visual v0.2.4 reconhecido");
}
MOD_EXTERN_FUNC void late_load() noexcept {
  LOGI("late_load v0.2.4: x +0.40 m, y +0.36 m, segue GameTable");
  createTargets();
}
