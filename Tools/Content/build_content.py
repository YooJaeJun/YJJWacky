"""에디터 Python(커맨드릿)으로 게임에 필요한 에셋을 만든다. 여러 번 실행해도 같은 결과.

만드는 것:
  /Game/YJJWacky/Materials/M_YJJ_Flat    텍스처 없는 단색 표면 (Color x 버텍스 컬러). 캐릭터·타워·땅 전부
  /Game/YJJWacky/Materials/M_YJJ_Unlit   반투명 단색 (사거리 원, 바람 줄기)
  /Game/YJJWacky/Materials/M_YJJ_Water   잔잔하게 일렁이는 물
  /Game/YJJWacky/Audio/*                 Tools/Audio/Generated/*.wav (generate_audio.py가 합성)
  /Game/YJJWacky/Maps/L_Arena            빈 맵 (게임모드가 전장을 생성)

실행: powershell -File Tools/Content/BuildContent.ps1 (에디터를 끈 상태)
"""

import glob
import os

import unreal

ROOT = "/Game/YJJWacky"
MAT_DIR = ROOT + "/Materials"
MAP_DIR = ROOT + "/Maps"
AUDIO_DIR = ROOT + "/Audio"

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
AUDIO_SRC = os.path.join(PROJECT_DIR, "Tools", "Audio", "Generated")

mel = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log("[BuildContent] " + msg)


def new_material(name):
    path = f"{MAT_DIR}/{name}"
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    material = asset_tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError("create failed " + path)
    return material


def node(material, cls, x, y):
    return mel.create_material_expression(material, cls, x, y)


def vector_param(material, name, color, x, y):
    p = node(material, unreal.MaterialExpressionVectorParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", unreal.LinearColor(*color))
    return p


def scalar_param(material, name, value, x, y):
    p = node(material, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", value)
    return p


def custom(material, code, inputs, output_type, x, y, desc):
    c = node(material, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("code", code)
    c.set_editor_property("description", desc)
    c.set_editor_property("output_type", output_type)
    pins = []
    for name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    c.set_editor_property("inputs", pins)
    return c


def connect(src, src_pin, dst, dst_pin):
    if not mel.connect_material_expressions(src, src_pin, dst, dst_pin):
        raise RuntimeError(f"connect failed -> {dst_pin}")


def output(src, src_pin, prop):
    if not mel.connect_material_property(src, src_pin, prop):
        raise RuntimeError(f"output failed {prop}")


def finish(material):
    if material.get_editor_property("material_domain") == unreal.MaterialDomain.MD_SURFACE:
        material.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(material)
    assets.save_loaded_asset(material)
    log("material " + material.get_path_name())


# ---------------------------------------------------------------- 단색 표면

def build_flat():
    m = new_material("M_YJJ_Flat")
    color = vector_param(m, "Color", (1, 1, 1, 1), -700, -100)
    emissive = scalar_param(m, "Emissive", 0.0, -700, 250)
    rough = scalar_param(m, "Roughness", 0.9, -700, 350)
    spec = scalar_param(m, "Specular", 0.2, -700, 450)
    vertex = node(m, unreal.MaterialExpressionVertexColor, -700, 100)

    base = node(m, unreal.MaterialExpressionMultiply, -400, 0)
    connect(color, "", base, "A")
    connect(vertex, "", base, "B")
    output(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

    glow = node(m, unreal.MaterialExpressionMultiply, -200, 200)
    connect(base, "", glow, "A")
    connect(emissive, "", glow, "B")
    output(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    output(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    output(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)


# ---------------------------------------------------------------- 반투명 단색

def build_unlit():
    m = new_material("M_YJJ_Unlit")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    color = vector_param(m, "Color", (1, 1, 1, 1), -600, -100)
    opacity = scalar_param(m, "Opacity", 0.5, -600, 150)
    bright = scalar_param(m, "Brightness", 1.6, -600, 250)
    mul = node(m, unreal.MaterialExpressionMultiply, -300, 0)
    connect(color, "", mul, "A")
    connect(bright, "", mul, "B")
    output(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    output(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    finish(m)


# ---------------------------------------------------------------- 물

WATER_CODE = r"""
// 큼직하고 느린 얼룩 두 겹으로 잔잔하게 일렁이는 단색 물
float a = sin(WP.x * 0.0017 + T * 0.45) * sin(WP.y * 0.0014 - T * 0.33);
float b = sin((WP.x + WP.y) * 0.0036 + T * 0.8) * sin((WP.x - WP.y) * 0.0029 - T * 0.6);
float band = saturate(0.5 + a * 0.35 + b * 0.15);
// 계단처럼 끊어서 로우폴리 느낌
band = floor(band * 4.0) / 4.0;
return lerp(Deep, Shallow, band);
"""


def build_water():
    m = new_material("M_YJJ_Water")
    deep = vector_param(m, "Deep", (0.07, 0.4, 0.72, 1), -900, -200)
    shallow = vector_param(m, "Shallow", (0.2, 0.66, 0.88, 1), -900, 0)
    rough = scalar_param(m, "Roughness", 0.35, -900, 200)
    time = node(m, unreal.MaterialExpressionTime, -900, 300)
    world_pos = node(m, unreal.MaterialExpressionWorldPosition, -900, 400)
    c = custom(m, WATER_CODE, ["Deep", "Shallow", "T", "WP"], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -500, 0, "Water")
    for pin, src in (("Deep", deep), ("Shallow", shallow), ("T", time), ("WP", world_pos)):
        connect(src, "", c, pin)
    output(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    output(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m)


# ---------------------------------------------------------------- 소리·맵

def import_sounds():
    files = sorted(glob.glob(os.path.join(AUDIO_SRC, "*.wav")))
    for path in files:
        name = os.path.splitext(os.path.basename(path))[0]
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", path)
        task.set_editor_property("destination_path", AUDIO_DIR)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        asset_tools.import_asset_tasks([task])
        sound = assets.load_asset(f"{AUDIO_DIR}/{name}")
        if sound is None:
            raise RuntimeError("sound import failed " + name)
        if name.startswith("BGM_"):
            sound.set_editor_property("looping", True)
        assets.save_loaded_asset(sound)
    log(f"sounds {len(files)}")


def make_map():
    path = MAP_DIR + "/L_Arena"
    if assets.does_asset_exist(path):
        log("map exists " + path)
        return
    world = asset_tools.create_asset("L_Arena", MAP_DIR, unreal.World, unreal.WorldFactory())
    assets.save_loaded_asset(world)
    log("map " + path)


def main():
    build_flat()
    build_unlit()
    build_water()
    import_sounds()
    make_map()
    assets.save_directory(ROOT, only_if_is_dirty=True, recursive=True)
    log("done")


main()
