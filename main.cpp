// Eleven Table Tennis / Quest 3 - three visual training targets (experimental)
// Native Scotland2 mod: runs at late_load, when Unity IL2CPP has initialized.
// Does not interact with ball launcher, scoring, tracking, or physics.
// IL2CPP runtime_invoke: value-type params point to value storage (e.g. &Vec3),
 // reference-type params point directly to Il2CppObject (not &objPtr).
#include <android/log.h>
#include <dlfcn.h>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include "scotland2/shared/modloader.h"

static constexpr const char* TAG = "ElevenTargets";
static CModInfo info{"eleven-quest-targets", "0.2.5", 0};
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
// Read Unity Il2CppString safely when comparing known ASCII scene names.
static bool unityStringEquals(void* str, const char* ascii) {
  if (!str || !ascii) return false;
  const auto* length=reinterpret_cast<const int32_t*>(
      reinterpret_cast<const uint8_t*>(str)+sizeof(Obj));
  size_t expected=std::strlen(ascii);
  if (*length < 0 || static_cast<size_t>(*length) != expected) return false;
  const auto* chars=reinterpret_cast<const uint16_t*>(
      reinterpret_cast<const uint8_t*>(str)+sizeof(Obj)+4);
  for(size_t i=0;i<expected;++i)
    if(chars[i] != static_cast<uint16_t>(static_cast<unsigned char>(ascii[i]))) return false;
  return true;
}

struct TableSurface {
  void* gameObject{};
  void* transform{};
  void* tableTransform{};
  void* tableGameObject{};
};

static TableSurface getTableSurface(Api& api, void* goClass,void* objectClass,
                                    void* transformClass,void* componentClass) {
  TableSurface result{};
  auto resources=api.klass("UnityEngine.CoreModule.dll","UnityEngine","Resources");
  auto getAll=api.method(resources,"FindObjectsOfTypeAll",1);
  auto getName=api.method(objectClass,"get_name",0);
  auto getTransform=api.method(goClass,"get_transform",0);
  auto getParent=api.method(transformClass,"get_parent",0);
  auto getGameObject=api.method(componentClass,"get_gameObject",0);
  auto getActive=api.method(goClass,"get_activeInHierarchy",0);
  if (!getAll||!getName||!getTransform||!getParent||!getGameObject) {
    LOGE("Impossivel validar a hierarquia GameTable / TableSurfaceSpace_forTargets");
    return result;
  }
  void* goType=api.type_get_object(api.class_get_type(goClass));
  LOGI("Buscando SurfaceSpace SOMENTE quando filho de GameTable ativo...");
  auto arr=reinterpret_cast<Array*>(call1(api,getAll,nullptr,goType));
  if(!arr||arr->max_length>100000) return result;
  int matches=0;
  for(uintptr_t i=0;i<arr->max_length;++i){
    void* candidate=arr->items[i];
    if(!candidate || !unityStringEquals(api.call(getName,candidate),
                                         "TableSurfaceSpace_forTargets")) continue;
    ++matches;
    void* surfaceTransform=api.call(getTransform,candidate);
    void* parent=surfaceTransform ? api.call(getParent,surfaceTransform) : nullptr;
    if(!parent || !unityStringEquals(api.call(getName,parent),"GameTable")) {
      LOGI("Candidato %d rejeitado: pai nao eh GameTable",matches);
      continue;
    }
    void* parentGo=api.call(getGameObject,parent);
    if (!parentGo) continue;
    if (getActive) {
      void* activeBox=api.call(getActive,parentGo);
      if (activeBox && !*reinterpret_cast<uint8_t*>(api.unbox(activeBox))){
        LOGI("Candidato %d rejeitado: GameTable inativo",matches);
        continue;
      }
    }
    result={candidate,surfaceTransform,parent,parentGo};
    LOGI("Referencia escolhida: SurfaceSpace filho de GameTable ATIVO, candidatos=%d",matches);
    return result;
  }
  LOGE("Nenhuma SurfaceSpace vinculada a GameTable ativo encontrada (%d candidatos)",matches);
  return result;
}

// Renderer.bounds supplies an axis-aligned world-space bounding box. Use the
// actual tabletop mesh(es) to find its physical top Y and horizontal center.
// We deliberately exclude tall net/leg meshes and narrow lines. If the game
// uses a custom renderer without suitable bounds, the original PC reference
// coordinates are used instead; no arbitrary fixed height is added.
struct Bounds { Vec3 center; Vec3 extents; };
struct TableGeometry {
  bool found=false;
  bool fullCenter=false;
  Vec3 center{};
  float topY=0.f;
  int parts=0;
};

static TableGeometry findTableGeometry(Api& api,void* tableGO,void* goClass,
                                        void* rendererClass) {
  TableGeometry geom{};
  if(!tableGO) return geom;
  auto children=api.method(goClass,"GetComponentsInChildren",2);
  auto getBounds=api.method(rendererClass,"get_bounds",0);
  if(!children||!getBounds){
    LOGI("Sem acesso a Renderer.bounds; usando a referencia do mod PC");
    return geom;
  }
  void* rendererType=api.type_get_object(api.class_get_type(rendererClass));
  bool includeInactive=false;
  void* args[]={rendererType,&includeInactive};
  auto list=reinterpret_cast<Array*>(api.call(children,tableGO,args));
  if(!list||list->max_length>5000){
    LOGI("Sem lista de Renderers de GameTable; usando referencia do mod PC");
    return geom;
  }
  struct Part { Vec3 center; float top; float area; };
  std::vector<Part> flat;
  for(uintptr_t i=0;i<list->max_length;++i){
    void* rend=list->items[i]; if(!rend)continue;
    void* boxed=api.call(getBounds,rend);
    if(!boxed)continue;
    Bounds b{};
    std::memcpy(&b,api.unbox(boxed),sizeof(b));
    const float sx=2.f*b.extents.x, sy=2.f*b.extents.y, sz=2.f*b.extents.z;
    const float area=sx*sz;
    if(!std::isfinite(area)||!std::isfinite(b.center.y)||
       !std::isfinite(sy)||sx<0.65f||sz<0.65f||
       sy>0.14f||area<0.85f||area>16.0f) continue;
    const float top=b.center.y+b.extents.y;
    flat.push_back({b.center,top,area});
    LOGI("Tampo candidato: centro=(%.3f,%.3f,%.3f), dim=(%.3f,%.3f,%.3f)",
         b.center.x,b.center.y,b.center.z,sx,sy,sz);
  }
  if(flat.empty()){
    LOGI("Nenhum Renderer plano compativel: usando origem SurfaceSpace do PC");
    return geom;
  }
  float top=-10000.f;
  for(const auto& p:flat)if(p.top>top)top=p.top;
  float sum=0.f;
  Vec3 center{0,0,0};
  int parts=0;
  float biggest=0;
  for(const auto& p:flat){
    if(std::fabs(p.top-top)>0.045f)continue;
    sum+=p.area;
    center.x+=p.center.x*p.area;
    center.z+=p.center.z*p.area;
    ++parts;
    if(p.area>biggest)biggest=p.area;
  }
  if(parts==0||sum<=0) return geom;
  center.x/=sum;
  center.z/=sum;
  center.y=top;
  geom.found=true;
  geom.fullCenter=(parts>=2 || biggest>2.8f);
  geom.center=center;
  geom.topY=top;
  geom.parts=parts;
  LOGI("Top real por Renderer.bounds: Y=%.3f; centro XZ=(%.3f,%.3f); partes=%d; centro valido=%s",
       top,center.x,center.z,parts,geom.fullCenter?"sim":"nao");
  return geom;
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
  TableSurface surface=getTableSurface(a,gameObject,object,transform,component);
  if(!surface.transform || !surface.tableTransform){
    LOGE("Cancelando: sem GameTable ativo, nao vamos criar alvos no espaco errado");
    return;
  }
  void* surfaceTransform=surface.transform;
  void* tableParent=surface.tableTransform;
  Quaternion q{0,0,0,1};
  Vec3 framePos{0,0,0};
  auto boxedRotation=a.call(getRotation,surfaceTransform);
  if(boxedRotation)std::memcpy(&q,a.unbox(boxedRotation),sizeof(q));
  auto boxedPosition=a.call(getPosition,surfaceTransform);
  if(boxedPosition)std::memcpy(&framePos,a.unbox(boxedPosition),sizeof(framePos));
  LOGI("PC SurfaceSpace world pos=(%.3f,%.3f,%.3f), rot=(%.3f,%.3f,%.3f,%.3f)",
       framePos.x,framePos.y,framePos.z,q.x,q.y,q.z,q.w);
  TableGeometry geometry=findTableGeometry(a,surface.tableGameObject,
                                            gameObject,renderer);
  Vec3 anchor=framePos;
  if(geometry.found){
    // Renderer.bounds is world space. This is the actual observed tabletop Y.
    anchor.y=geometry.topY;
    if(geometry.fullCenter){
      anchor.x=geometry.center.x;
      anchor.z=geometry.center.z;
    }
  }
  LOGI("Ancora final dos alvos=(%.3f,%.3f,%.3f), geometria=%s",
       anchor.x,anchor.y,anchor.z,geometry.found?"tampo real":"SurfaceSpace");
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
    // The PC mod's physical, unscaled spacing: (-.46,0,+.46) and +.86 on Z.
    // Anchor Y from actual tabletop Renderers, without arbitrary offsets.
    // Parent will follow GameTable after its VR recentering.
    Vec3 local{xs[i],0.008f,0.86f};
    const Vec3 rotated=rotateByQuaternion(q,local);
    Vec3 world{anchor.x+rotated.x,
               anchor.y+rotated.y,
               anchor.z+rotated.z};
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
  LOGI("setup: modulo visual v0.2.5 reconhecido");
}
MOD_EXTERN_FUNC void late_load() noexcept {
  LOGI("late_load v0.2.5: referencia GameTable validada, altura pelo tampo, sem offsets arbitrarios");
  createTargets();
}
