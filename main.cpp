// Eleven Table Tennis / Quest 3 - v0.3.3 experimental: visible digit display for Backhand 30 Bolas
// Native Scotland2 mod: runs at late_load, when Unity IL2CPP has initialized.
// Only limits launches when the BH30C0 preset is the sole active saved shot.
// IL2CPP runtime_invoke: value-type params point to value storage (e.g. &Vec3),
 // reference-type params point directly to Il2CppObject (not &objPtr).
#include <android/log.h>
#include <dlfcn.h>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <iterator>
#include "dobby.h"
#include "scotland2/shared/modloader.h"

static constexpr const char* TAG = "ElevenTargets";
static CModInfo info{"eleven-quest-targets", "0.3.3", 0};
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

// Name a Unity object in the log, keeping the candidate-chain inspectable.
static std::string unityNameAscii(void* str) {
  if (!str) return "<null>";
  const auto* length=reinterpret_cast<const int32_t*>(
       reinterpret_cast<const uint8_t*>(str)+sizeof(Obj));
  if (*length<0 || *length>160) return "<invalid-name>";
  const auto* chars=reinterpret_cast<const uint16_t*>(
       reinterpret_cast<const uint8_t*>(str)+sizeof(Obj)+4);
  std::string result; result.reserve(*length);
  for (int i=0;i<*length;++i)
    result.push_back(chars[i]>=32 && chars[i]<127 ? static_cast<char>(chars[i]) : '?');
  return result;
}
static bool isGameTableName(const std::string& name) {
  // Tolerate runtime clones, without confusing unrelated named objects.
  return name=="GameTable" || name=="GameTable(Clone)" ||
         name=="GameTable (Clone)";
}
static bool getWorldPosition(Api& api,void* tr,void* getPosition,Vec3& out) {
  if (!tr || !getPosition) return false;
  auto boxed=api.call(getPosition,tr);
  if (!boxed) return false;
  std::memcpy(&out,api.unbox(boxed),sizeof(out));
  return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
}

static TableSurface getTableSurface(Api& api, void* goClass,void* objectClass,
                                     void* transformClass,void* componentClass) {
  TableSurface result{};
  auto resources=api.klass("UnityEngine.CoreModule.dll","UnityEngine","Resources");
  auto getAll=api.method(resources,"FindObjectsOfTypeAll",1);
  auto getName=api.method(objectClass,"get_name",0);
  auto getTransform=api.method(goClass,"get_transform",0);
  auto getParent=api.method(transformClass,"get_parent",0);
  auto getGameObject=api.method(componentClass,"get_gameObject",0);
  auto getPosition=api.method(transformClass,"get_position",0);
  auto getActive=api.method(goClass,"get_activeInHierarchy",0);
  if (!getAll || !getName || !getTransform || !getParent || !getGameObject) {
    LOGE("Nao foi possivel enumerar SurfaceSpace e hierarquia da mesa");
    return result;
  }
  void* goType=api.type_get_object(api.class_get_type(goClass));
  auto arr=reinterpret_cast<Array*>(call1(api,getAll,nullptr,goType));
  if (!arr || arr->max_length>100000) {
    LOGE("Lista de GameObjects indisponivel/invalida");
    return result;
  }
  LOGI("Enumerando SurfaceSpaces e ancestrais; nao exige GameTable como pai direto");
  int matches=0;
  // Use the original known-working reference when there is only one candidate.
  for(uintptr_t i=0;i<arr->max_length;++i) {
    void* candidate=arr->items[i];
    if (!candidate || !unityStringEquals(api.call(getName,candidate),
                                          "TableSurfaceSpace_forTargets")) continue;
    ++matches;
    void* surfaceTransform=api.call(getTransform,candidate);
    if (!surfaceTransform) continue;
    TableSurface current{candidate,surfaceTransform,nullptr,nullptr};
    void* tr=api.call(getParent,surfaceTransform);
    if (tr) current.tableTransform=tr; // Fallback: nearest parent follows VR table movement.
    std::string path;
    for(int depth=1;tr && depth<=16;++depth) {
      std::string n=unityNameAscii(api.call(getName,tr));
      if (!path.empty()) path += " < ";
      path += n;
      if (isGameTableName(n)) {
        void* parentGo=api.call(getGameObject,tr);
        bool active=true;
        if (getActive && parentGo) {
          void* box=api.call(getActive,parentGo);
          if (box) active=(*reinterpret_cast<uint8_t*>(api.unbox(box)) != 0);
        }
        LOGI("Candidato %d: GameTable ancestral no nivel %d; ativo=%s",
             matches,depth,active?"sim":"nao");
        if (active && parentGo) {
          current.tableTransform=tr;
          current.tableGameObject=parentGo;
          LOGI("Candidato %d: cadeia=%s",matches,path.c_str());
          return current;
        }
      }
      tr=api.call(getParent,tr);
    }
    LOGI("Candidato %d: cadeia de pais=%s",matches,path.c_str());
    if (!result.transform) result=current;
  }
  if (!result.transform) {
    LOGE("TableSurfaceSpace_forTargets nao encontrada");
    return result;
  }
  LOGI("SurfaceSpace encontrada (%d candidatos), sem GameTable ancestral confirmado",matches);

  // In the Quest scene the table can have its own transform hierarchy.
  // Try to find a nearby *active* GameTable by name; never make it mandatory.
  Vec3 surfacePos{};
  bool haveSurfacePos=getWorldPosition(api,result.transform,getPosition,surfacePos);
  float bestDist2=1.0e20f;
  int tables=0;
  for(uintptr_t i=0;i<arr->max_length;++i) {
    void* candidate=arr->items[i]; if (!candidate) continue;
    auto name=unityNameAscii(api.call(getName,candidate));
    if (!isGameTableName(name)) continue;
    ++tables;
    void* tr=api.call(getTransform,candidate);
    Vec3 p{};
    if (!getWorldPosition(api,tr,getPosition,p) || !haveSurfacePos) continue;
    bool active=true;
    if (getActive) {
      auto box=api.call(getActive,candidate);
      if (box) active=(*reinterpret_cast<uint8_t*>(api.unbox(box)) != 0);
    }
    float dx=p.x-surfacePos.x,dy=p.y-surfacePos.y,dz=p.z-surfacePos.z;
    float dist2=dx*dx+dy*dy+dz*dz;
    LOGI("GameTable avulsa: dist=%.3f m, ativo=%s",
          std::sqrt(dist2),active?"sim":"nao");
    if (active && dist2<bestDist2 && dist2<16.0f) {
      bestDist2=dist2;
      result.tableGameObject=candidate;
      result.tableTransform=tr;
    }
  }
  if (result.tableGameObject)
    LOGI("Usando GameTable ativa proxima da SurfaceSpace (dist=%.3f m)",
         std::sqrt(bestDist2));
  else
    LOGI("Sem GameTable proxima (%d candidatas); manter SurfaceSpace original sem bloquear criacao",tables);
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
  if(!surface.transform){
    LOGE("Cancelando: SurfaceSpace nao encontrada");
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

// ============================================================================
// v0.3.2: opt-in countdown and 30-shot limit for the Backhand 30 Bolas preset.
// The *existing* ball launcher is responsible for ball speed, trajectory and
// timing. We never manufacture ball physics or alter the established targets.
// Hook functions must run on the Unity thread: no background Unity API calls.
// ============================================================================
static constexpr int kCountdownSeconds = 15;
static constexpr int kSeriesLaunches = 30;
enum class DrillState : uint8_t { Idle, Countdown, Running, Finished };
enum class PresetSelection : uint8_t { Unavailable, Other, Backhand30 };
using LauncherFn = void (*)(void* self, const void* methodInfo);
using SteadyClock = std::chrono::steady_clock;

struct Segment {
  void* gameObject{};
  void* renderer{}; // Keep the object active; toggle MeshRenderer.enabled instead.
};
struct Drill {
  Api api{};
  DrillState state{DrillState::Idle};
  void* activeLauncher{};
  SteadyClock::time_point startTime{};
  int launches{0};
  int shown{-2};
  int printedSecond{-2};
  Segment segments[2][7]{};
  void* setActive{}; // Fallback only for unusual Unity builds.
  void* setRendererEnabled{};
  bool displayReady{false};
  PresetSelection selection{PresetSelection::Unavailable};
  SteadyClock::time_point lastSelectionPoll{};
  bool selectionLogged{false};
  uint64_t updatesSeen{0};
  uint64_t launchesSeen{0};
} ;
static Drill drill;
static LauncherFn originalUpdate = nullptr;
static LauncherFn originalLaunchBall = nullptr;
static LauncherFn originalOnDisable = nullptr;
static LauncherFn originalOnEnable = nullptr;


// Optional test-only path override. Production uses the game's own Android files.
// Never alter this JSON: it is owned and saved by the game's Ball Machine UI.
static const char* settingsTestPath = nullptr;

// Determine whether a JSON key's value is True (the game stores booleans as
// strings). This operates on a bounded snippet, not on arbitrary Unity memory.
static bool boolTrueAt(const std::string& json,size_t keyAt,size_t end) {
  const auto colon=json.find(':',keyAt);
  if(colon==std::string::npos||colon>=end) return false;
  size_t j=colon+1;
  while(j<end && (json[j]==' '||json[j]=='\t'||json[j]=='\n'||json[j]=='\r')) ++j;
  if(j>=end) return false;
  if(json[j]=='"') {
    ++j;
    return j+4<=end && json.compare(j,4,"True")==0 && j+4<end && json[j+4]=='"';
  }
  return j+4<=end && json.compare(j,4,"true")==0 &&
         (j+4==end || json[j+4]==',' || json[j+4]=='}' || json[j+4]==' ');
}

static size_t objectEndAt(const std::string& json,size_t open) {
  if(open>=json.size()||json[open]!='{') return std::string::npos;
  int depth=0;bool quoted=false,escaped=false;
  for(size_t i=open;i<json.size();++i){
    char c=json[i];
    if(quoted) {
      if(escaped) escaped=false;
      else if(c=='\\') escaped=true;
      else if(c=='"') quoted=false;
      continue;
    }
    if(c=='"'){quoted=true;continue;}
    if(c=='{') ++depth;
    if(c=='}' && --depth==0) return i+1;
  }
  return std::string::npos;
}

// Pure parser, covered by host tests. Safety rule: only control the machine
// when exactly ONE preset is active and its stable ID is BH30C0.
static PresetSelection selectionFromJson(const std::string& json) {
  const auto id=json.find("\"BH30C0\"");
  if(id==std::string::npos) return PresetSelection::Other;
  const auto open=json.find('{',id+8);
  if(open==std::string::npos) return PresetSelection::Other;
  const auto end=objectEndAt(json,open);
  if(end==std::string::npos) return PresetSelection::Other;
  const auto activeKey=json.find("\"isActive\"",open);
  if(activeKey==std::string::npos||activeKey>=end||!boolTrueAt(json,activeKey,end))
    return PresetSelection::Other;
  size_t nActive=0,pos=0;
  while((pos=json.find("\"isActive\"",pos))!=std::string::npos) {
    if(boolTrueAt(json,pos,json.size())) ++nActive;
    pos+=10;
  }
  return nActive==1 ? PresetSelection::Backhand30 : PresetSelection::Other;
}

static PresetSelection readPresetFromFile(const char** usedPath) {
  static const char* paths[]={
    "/sdcard/Android/data/quest.eleven.forfunlabs/settings/BallLauncherSettings.json",
    "/sdcard/Android/data/quest.eleven.forfunlabs/files/settings/BallLauncherSettings.json",
    "/storage/emulated/0/Android/data/quest.eleven.forfunlabs/settings/BallLauncherSettings.json",
    "/storage/emulated/0/Android/data/quest.eleven.forfunlabs/files/settings/BallLauncherSettings.json"
  };
  if(usedPath) *usedPath=nullptr;
  for(size_t i=0;i<(settingsTestPath?1:4);++i){
    const char* path=settingsTestPath?settingsTestPath:paths[i];
    std::ifstream file(path,std::ios::binary);
    if(!file) continue;
    std::string json((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    if(usedPath) *usedPath=path;
    if(json.empty() || json.size()>1024*1024) return PresetSelection::Unavailable;
    return selectionFromJson(json);
  }
  return PresetSelection::Unavailable;
}

static const char* selectionName(PresetSelection s) {
  switch(s){
    case PresetSelection::Backhand30: return "Backhand 30 Bolas";
    case PresetSelection::Other: return "outro exercicio";
    default: return "JSON indisponivel";
  }
}

// Segments: top, top-left, top-right, middle, bottom-left, bottom-right, bottom.
// These are ordinary non-colliding Unity cubes: the geometry reliably worked
// on Quest for the targets, avoiding extra TMPro/Canvas dependencies.
static constexpr bool sevenSegment[10][7] = {
 {1,1,1,0,1,1,1}, // 0
 {0,0,1,0,0,1,0}, // 1
 {1,0,1,1,1,0,1}, // 2
 {1,0,1,1,0,1,1}, // 3
 {0,1,1,1,0,1,0}, // 4
 {1,1,0,1,0,1,1}, // 5
 {1,1,0,1,1,1,1}, // 6
 {1,0,1,0,0,1,0}, // 7
 {1,1,1,1,1,1,1}, // 8
 {1,1,1,1,0,1,1}  // 9
};

// Keep GameObjects alive and visible to the scene hierarchy. On Quest 3 the
// original display was created successfully but no numbers were seen. Toggling
// Renderer.enabled avoids repeated activation/deactivation of each object.
static void displayValue(int value) {
  if (!drill.displayReady || (!drill.setRendererEnabled && !drill.setActive) ||
      drill.shown == value) return;
  int enabledCount=0;
  int errors=0;
  for(int digit=0;digit<2;++digit){
    int d = (digit==0 ? value/10 : value%10);
    if(value<0) d=-1;
    // Single digit for 0..9; two digits for 10..30.
    if(digit==0 && value>=0 && value<10) d=-1;
    for(int seg=0;seg<7;++seg){
      const Segment& part=drill.segments[digit][seg];
      bool enabled = d>=0 && d<=9 && sevenSegment[d][seg];
      if(enabled) ++enabledCount;
      const bool ok = (drill.setRendererEnabled && part.renderer)
        ? write1(drill.api,drill.setRendererEnabled,part.renderer,&enabled)
        : (drill.setActive && part.gameObject &&
           write1(drill.api,drill.setActive,part.gameObject,&enabled));
      if(!ok) ++errors;
    }
  }
  if(errors==0) drill.shown=value;
  LOGI("Treino v0.3.3: visor valor=%d segmentos_ligados=%d erros=%d modo=%s",
       value,enabledCount,errors,drill.setRendererEnabled?"Renderer":"SetActive-fallback");
}

static void createCountdownDisplay() {
  auto& a = drill.api;
  if(!a.init()){ LOGE("Treino v0.3: IL2CPP indisponivel para visor"); return; }
  auto goClass=a.klass("UnityEngine.CoreModule.dll","UnityEngine","GameObject");
  auto trClass=a.klass("UnityEngine.CoreModule.dll","UnityEngine","Transform");
  auto objClass=a.klass("UnityEngine.CoreModule.dll","UnityEngine","Object");
  auto componentClass=a.klass("UnityEngine.CoreModule.dll","UnityEngine","Component");
  auto rendererClass=a.klass("UnityEngine.CoreModule.dll","UnityEngine","Renderer");
  auto materialClass=a.klass("UnityEngine.CoreModule.dll","UnityEngine","Material");
  auto colliderClass=a.klass("UnityEngine.PhysicsModule.dll","UnityEngine","Collider");
  if(!goClass||!trClass||!objClass||!componentClass||!rendererClass||!materialClass||!colliderClass){
    LOGE("Treino v0.3: classes de visor nao encontradas");return;
  }
  auto primitive=a.method(goClass,"CreatePrimitive",1);
  auto getTransform=a.method(goClass,"get_transform",0);
  auto setPosition=a.method(trClass,"set_position",1);
  auto setRotation=a.method(trClass,"set_rotation",1);
  auto setScale=a.method(trClass,"set_localScale",1);
  auto setParent=a.method(trClass,"SetParent",2);
  auto getPosition=a.method(trClass,"get_position",0);
  auto getRotation=a.method(trClass,"get_rotation",0);
  auto getComponent=a.method(goClass,"GetComponent",1);
  auto setEnabled=a.method(colliderClass,"set_enabled",1);
  auto getMaterial=a.method(rendererClass,"get_material",0);
  auto setColor=a.method(materialClass,"set_color",1);
  auto destroy=a.method(objClass,"Destroy",1);
  drill.setActive=a.method(goClass,"SetActive",1);
  drill.setRendererEnabled=a.method(rendererClass,"set_enabled",1);
  if(!primitive||!getTransform||!setPosition||!setRotation||!setScale||!getComponent||!setEnabled||!getMaterial||!setColor||(!drill.setRendererEnabled&&!drill.setActive)){
    LOGE("Treino v0.3: metodos do visor faltando; treino sem numeros visuais");return;
  }
  auto surface=getTableSurface(a,goClass,objClass,trClass,componentClass);
  if(!surface.transform){ LOGE("Treino v0.3: mesa nao encontrada para visor"); return; }
  Quaternion q{0,0,0,1}; Vec3 anchor{};
  auto bq=a.call(getRotation,surface.transform);
  auto bp=a.call(getPosition,surface.transform);
  if(bq) std::memcpy(&q,a.unbox(bq),sizeof(q));
  if(bp) std::memcpy(&anchor,a.unbox(bp),sizeof(anchor));
  auto geom=findTableGeometry(a,surface.tableGameObject,goClass,rendererClass);
  if(geom.found){
    anchor.y=geom.topY;
    if(geom.fullCenter){anchor.x=geom.center.x; anchor.z=geom.center.z;}
  }
  // Two 7-segment digits face the player from above the net, floating 40cm.
  // Positions are physical meters relative to the same working table anchor.
  struct SegmentGeom { float x,y; bool horizontal; };
  static constexpr SegmentGeom shapes[7] = {
     {0.00f, 0.12f, true}, {-0.065f, 0.063f, false},
     {0.065f, 0.063f, false}, {0.00f, 0.0f, true},
     {-0.065f, -0.063f, false}, {0.065f, -0.063f, false},
     {0.00f, -0.12f, true}
  };
  void* colliderType=a.type_get_object(a.class_get_type(colliderClass));
  void* rendererType=a.type_get_object(a.class_get_type(rendererClass));
  LOGI("Treino v0.3.3: visor usando %s",drill.setRendererEnabled?"Renderer.enabled":"SetActive-fallback");
  int count=0;
  for(int digit=0;digit<2;++digit){
    for(int seg=0;seg<7;++seg){
      int cube=3; // UnityEngine.PrimitiveType.Cube
      void* obj=call1(a,primitive,nullptr,&cube);
      if(!obj){LOGE("Treino v0.3: falha ao criar segmento %d/%d",digit,seg); continue;}
      // Keep the GameObject active so Unity can keep tracking its Renderer.
      // Disable its Renderer immediately; avoid SetActive(false) on Quest.
      bool enabled=false;
      void* render=rendererType ? call1(a,getComponent,obj,rendererType) : nullptr;
      if(!render || !(drill.setRendererEnabled
             ? write1(a,drill.setRendererEnabled,render,&enabled)
             : write1(a,drill.setActive,obj,&enabled))){
        LOGE("Treino v0.3.3: nao consegui ocultar segmento inicial");
        if(destroy) call1(a,destroy,nullptr,obj);
        continue;
      }
      // In particular, never leave a newly-created cube with a live collider.
      void* collider=call1(a,getComponent,obj,colliderType);
      if(!collider || !write1(a,setEnabled,collider,&enabled)){
        LOGE("Treino v0.3: collider do visor nao desativado; descartando segmento");
        if(destroy) call1(a,destroy,nullptr,obj);
        continue;
      }
      void* tr=a.call(getTransform,obj);
      if(!tr){if(destroy)call1(a,destroy,nullptr,obj);continue;}
      const auto& shape=shapes[seg];
      // Larger/clearer display, 60 cm over the net area (same calibrated
      // SurfaceSpace anchor used by the targets; target coordinates unchanged).
      Vec3 offset{(digit?0.10f:-0.10f)+shape.x,0.60f+shape.y,0.20f};
      Vec3 rot=rotateByQuaternion(q,offset);
      Vec3 pos{anchor.x+rot.x,anchor.y+rot.y,anchor.z+rot.z};
      Vec3 scale=shape.horizontal ? Vec3{0.12f,0.022f,0.012f}
                                  : Vec3{0.022f,0.108f,0.012f};
      bool positioned=write1(a,setPosition,tr,&pos) &&
                      write1(a,setScale,tr,&scale) &&
                      (!setRotation || write1(a,setRotation,tr,&q));
      auto mat=a.call(getMaterial,render);
      Color yellow{1.f,0.91f,0.09f,1.f};
      if(!positioned || !mat || !write1(a,setColor,mat,&yellow)){
        LOGE("Treino v0.3: segmento sem posicao/cor valida; descartando");
        if(destroy) call1(a,destroy,nullptr,obj);
        continue;
      }
      if(surface.tableTransform&&setParent){
        bool keepWorld=true;void* args[]={surface.tableTransform,&keepWorld};
        a.call(setParent,tr,args);
      }
      // Renderer starts hidden; GameObject remains active in the scene.
      drill.segments[digit][seg]={obj,render};
      ++count;
    }
  }
  drill.displayReady=(count==14);
  LOGI("Treino v0.3.3: visor 7 segmentos criado: %d/14; %s",
       count,drill.displayReady?"pronto":"parcial");
}

// Called only on the Unity thread. Polling at most once per second avoids
// re-reading a configuration file for each rendered frame.
static void refreshPreset(bool force=false) {
  auto now=SteadyClock::now();
  if(!force && drill.lastSelectionPoll!=SteadyClock::time_point{} &&
     now-drill.lastSelectionPoll<std::chrono::seconds(1)) return;
  drill.lastSelectionPoll=now;
  const char* path=nullptr;
  PresetSelection selected=readPresetFromFile(&path);
  if(drill.selectionLogged && selected==drill.selection) return;
  drill.selectionLogged=true;
  LOGI("Treino v0.3.2: preset detectado: %s -> %s; arquivo=%s",
       selectionName(drill.selection),selectionName(selected),
       path?path:"NAO ENCONTRADO");
  // Re-selection must start a fresh 15s countdown; do not affect the game
  // when the preset is different or the file is unavailable.
  drill.activeLauncher=nullptr;
  drill.state=DrillState::Idle;
  drill.launches=0;
  drill.printedSecond=-2;
  displayValue(-1);
  drill.selection=selected;
}

static int countdownRemaining() {
  using seconds=std::chrono::duration<double>;
  const auto elapsed=std::chrono::duration_cast<seconds>(SteadyClock::now()-drill.startTime).count();
  return std::clamp(static_cast<int>(std::ceil(kCountdownSeconds-elapsed)),0,kCountdownSeconds);
}
static void hookedUpdate(void* self,const void* mi) {
  // Required diagnostic: verifies that the hook is invoked during gameplay.
  ++drill.updatesSeen;
  if(drill.updatesSeen==1 || drill.updatesSeen%1800==0)
    LOGI("Treino v0.3.2: hook Update executado, chamadas=%llu",
         static_cast<unsigned long long>(drill.updatesSeen));
  refreshPreset();
  if(originalUpdate) originalUpdate(self,mi);
  if(drill.selection!=PresetSelection::Backhand30 ||
     self!=drill.activeLauncher || drill.state==DrillState::Idle) return;
  if(drill.state==DrillState::Countdown){
    int left=countdownRemaining();
    displayValue(left);
    if(left!=drill.printedSecond){
      LOGI("Treino v0.3: inicio em %d s",left); drill.printedSecond=left;
    }
    if(left==0){
      drill.state=DrillState::Running;
      LOGI("Treino v0.3: contagem encerrada; lancamentos liberados");
      displayValue(kSeriesLaunches);
    }
  }
}
static void hookedLaunch(void* self,const void* mi) {
  if(!originalLaunchBall) return;
  ++drill.launchesSeen;
  refreshPreset(true);
  if(drill.launchesSeen<=5 || drill.launchesSeen%30==0)
    LOGI("Treino v0.3.2: hook LaunchBall chamado %llu vezes; preset=%s",
         static_cast<unsigned long long>(drill.launchesSeen),
         selectionName(drill.selection));
  // Never block balls in any other preset, including when the JSON is absent.
  if(drill.selection!=PresetSelection::Backhand30) {
    originalLaunchBall(self,mi);
    return;
  }
  if(drill.state==DrillState::Idle || drill.activeLauncher!=self){
    drill.activeLauncher=self;
    drill.launches=0;
    drill.state=DrillState::Countdown;
    drill.startTime=SteadyClock::now();
    drill.printedSecond=-2;
    displayValue(kCountdownSeconds);
    LOGI("Treino v0.3: primeira tentativa de lancamento: contagem de 15 s iniciada");
    return; // Block first ball while counting down.
  }
  if(drill.state==DrillState::Countdown){
    if(countdownRemaining()>0) return;
    drill.state=DrillState::Running;
    LOGI("Treino v0.3: contagem concluida (chamada LaunchBall)");
  }
  if(drill.state==DrillState::Finished || drill.launches>=kSeriesLaunches) return;
  originalLaunchBall(self,mi); // Existing launcher retains its interval and ball settings.
  ++drill.launches;
  LOGI("Treino v0.3: LaunchBall %d/30",drill.launches);
  displayValue(kSeriesLaunches-drill.launches);
  if(drill.launches>=kSeriesLaunches){
    drill.state=DrillState::Finished;
    LOGI("Treino v0.3: 30/30; novos lancamentos bloqueados ate desligar o lancador");
  }
}
static void resetSeries(void* self,const char* reason){
  if(self!=drill.activeLauncher) return;
  drill.activeLauncher=nullptr;
  drill.state=DrillState::Idle;
  drill.launches=0;
  drill.printedSecond=-2;
  displayValue(-1);
  LOGI("Treino v0.3: %s; pronto para nova serie",reason);
}
static void hookedOnDisable(void* self,const void* mi) {
  if(originalOnDisable) originalOnDisable(self,mi);
  LOGI("Treino v0.3.2: BallLauncher.OnDisable recebido");
  resetSeries(self,"OnDisable");
}
static void hookedOnEnable(void* self,const void* mi) {
  if(originalOnEnable) originalOnEnable(self,mi);
  LOGI("Treino v0.3.2: BallLauncher.OnEnable recebido");
  // Fallback when the component is reactivated without a preceding OnDisable.
  resetSeries(self,"OnEnable");
}
static void* methodPointer(Api& api,void* klass,const char* name,bool required=true){
  void* method=api.method(klass,name,0);
  if(!method){
    if(required) LOGE("Treino v0.3: metodo %s nao encontrado",name);
    else LOGI("Treino v0.3: evento opcional %s indisponivel",name);
    return nullptr;
  }
  // Unity 2022 MethodInfo begins with Il2CppMethodPointer.
  void* pointer=nullptr;std::memcpy(&pointer,method,sizeof(pointer));
  Dl_info dli{};
  if(!pointer || dladdr(pointer,&dli)==0 || !dli.dli_fname ||
     !std::strstr(dli.dli_fname,"libil2cpp.so")) {
    LOGE("Treino v0.3: ponteiro de %s nao validado, sem hook",name);
    return nullptr;
  }
  LOGI("Treino v0.3: metodo %s em %p",name,pointer);
  return pointer;
}
static void installTraining() {
  // Scotland2 late_load runs on the Unity main thread after IL2CPP init.
  // Fail closed if any required hook is unavailable: never launch balls ourselves.
  auto& a=drill.api;
  if(!a.init()){LOGE("Treino v0.3: falha na API IL2CPP");return;}
  auto launcher=a.klass("Eleven.dll","ELEVEN","BallLauncher");
  if(!launcher) launcher=a.klass("Assembly-CSharp.dll","ELEVEN","BallLauncher");
  if(!launcher){LOGE("Treino v0.3: ELEVEN.BallLauncher nao encontrado");return;}
  auto update=methodPointer(a,launcher,"Update");
  auto launch=methodPointer(a,launcher,"LaunchBall");
  auto disable=methodPointer(a,launcher,"OnDisable",false);
  auto enable=methodPointer(a,launcher,"OnEnable",false);
  if(!update||!launch){LOGE("Treino v0.3: hooks obrigatorios indisponiveis");return;}
  // A countdown that is invisible can confuse players. Do not intercept the
  // real ball launcher unless the complete, non-colliding yellow display exists.
  createCountdownDisplay();
  if(!drill.displayReady){
    LOGE("Treino v0.3: visor incompleto; lancador original NAO sera alterado");
    return;
  }
  // Install Update first to keep countdown refreshed every frame.
  int statusUpdate=DobbyHook(update,(void*)hookedUpdate,(void**)&originalUpdate);
  if(statusUpdate!=0 || !originalUpdate){
    LOGE("Treino v0.3: DobbyHook Update falhou: %d",statusUpdate);return;
  }
  int statusLaunch=DobbyHook(launch,(void*)hookedLaunch,(void**)&originalLaunchBall);
  if(statusLaunch!=0 || !originalLaunchBall){
    LOGE("Treino v0.3: DobbyHook LaunchBall falhou: %d",statusLaunch);
    // Undo first hook, since an Update hook without launch limiting is useless.
    DobbyDestroy(update); originalUpdate=nullptr;return;
  }
  bool hasResetHook=false;
  if(disable){
    int s=DobbyHook(disable,(void*)hookedOnDisable,(void**)&originalOnDisable);
    LOGI("Treino v0.3: hook OnDisable=%d",s);
    hasResetHook=(s==0 && originalOnDisable);
  }
  if(enable){
    int s=DobbyHook(enable,(void*)hookedOnEnable,(void**)&originalOnEnable);
    LOGI("Treino v0.3: hook OnEnable=%d",s);
    hasResetHook=hasResetHook || (s==0 && originalOnEnable);
  }
  if(!hasResetHook){
    LOGE("Treino v0.3.2: sem evento de reset, escolha outro preset e volte para reiniciar");
  }
  LOGI("Treino v0.3.3: hooks prontos, limite de 30 SOMENTE se BH30C0 for o unico preset ativo salvo");
  refreshPreset(true);
}

} // namespace

MOD_EXTERN_FUNC void setup(CModInfo* output) noexcept {
  *output=info;
  LOGI("setup: v0.3.3 alvos preservados; visor corrigido com Renderer.enabled");
}
MOD_EXTERN_FUNC void late_load() noexcept {
  LOGI("late_load v0.3.3: alvos v0.2.6 inalterados; visor Renderer.enabled");
  createTargets();
  installTraining();
}
