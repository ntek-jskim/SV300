"""Keil uvproj 타깃 → compile_commands.json (clangd 용).
usage: python tools/gen_compile_commands.py keil/SV300.uvproj SV300_3CH_16M compile_commands.json
uvproj 의 소스·define·include 가 바뀌면 다시 실행. 경로는 C:\\Keil_v473, LLVM 23 기준.
"""
import json, os, sys
import xml.etree.ElementTree as ET

uvproj, target, out = sys.argv[1], sys.argv[2], sys.argv[3]
pdir = os.path.dirname(os.path.abspath(uvproj))
root = ET.parse(uvproj).getroot()
t = next(x for x in root.iter("Target") if x.findtext("TargetName") == target)
ca = t.find(".//TargetArmAds/Cads/VariousControls")
defs = (ca.findtext("Define") or "").split()
incs = [i for i in (ca.findtext("IncludePath") or "").split(";") if i]

KEIL = r"C:\Keil_v473\ARM"
args = ["clang", "--target=arm-none-eabi", "-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=hard",
        "-std=gnu99", "-ferror-limit=0",
        # clang 은 __GNUC__ 를 정의하므로 CMSIS 는 GCC 경로를 탄다. armcc 전용 키워드만 흉내
        "-D__packed=__attribute__((packed))", "-D__irq=", "-D__value_in_regs=",
        "-D__weak=__attribute__((weak))", "-D__align(x)=__attribute__((aligned(x)))",
        "-D__svc(x)=", "-D__svc_indirect(x)=", "-D__swi(x)=", "-D__task=", "-D__forceinline=inline"]
args += ["-D" + d for d in defs]
args += ["-I" + os.path.normpath(os.path.join(pdir, i)) for i in incs]
args += ["-I" + KEIL + r"\RV31\INC"]	# RTL.h 등 Keil RL-ARM 헤더
# 표준 C 헤더: arm-none-eabi 용 libc 가 없으므로 Keil ARMCC 헤더 사용 (armcc 전용 지정자는 빈 값)
# clang 내장 헤더(stddef/stdint/stdarg 등)를 먼저 — ARMCC stddef.h 의 offsetof 는 armcc 내장 __INTADDR__ 에 의존
args += ["-isystem", r"C:\Program Files\LLVM\lib\clang\23\include", "-ffreestanding", "-fshort-wchar"]	# armcc wchar_t = unsigned short
args += ["-isystem", KEIL + r"\ARMCC\include",
         "-D__declspec(x)=", "-D__pure=", "-D__softfp=", "-D__smc(x)=", "-D__global_reg(x)=",
         "-D__int64=long long", "-D__ESCAPE__(x)=(x)"]

db = []
for f in t.iter("File"):
    if f.findtext("FileType") != "1":	# 1 = C 소스
        continue
    src = os.path.normpath(os.path.join(pdir, f.findtext("FilePath")))
    db.append({"directory": pdir, "file": src, "arguments": args + ["-c", src]})

json.dump(db, open(out, "w", encoding="utf-8"), indent=1)
print(len(db), "entries ->", out)
