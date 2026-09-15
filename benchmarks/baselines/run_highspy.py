"""Run an optional, separately installed HiGHS reference outside AstraNiti."""
from __future__ import annotations
import argparse,json,math,platform,sys,time
from pathlib import Path

def finite(value): return value if isinstance(value,(int,float)) and math.isfinite(value) else None
def run(model:Path,module_path:Path|None=None)->dict:
    if module_path is not None: sys.path.insert(0,str(module_path))
    try: import highspy
    except ImportError: return {"label":"REFERENCE","solver":"HiGHS","available":False,"executed":False,"model":str(model),"objective":None,"error":"highspy is not installed in the supplied isolated module path"}
    solver=highspy.Highs();solver.setOptionValue("output_flag",False);started=time.perf_counter();read=solver.readModel(str(model.resolve()));status=solver.run();elapsed=time.perf_counter()-started;info=solver.getInfo()
    version=f"{highspy.HIGHS_VERSION_MAJOR}.{highspy.HIGHS_VERSION_MINOR}.{highspy.HIGHS_VERSION_PATCH}"
    return {"label":"REFERENCE","solver":"HiGHS","version":version,"available":True,"executed":True,"model":str(model),"read_status":str(read),"run_status":str(status),"model_status":solver.modelStatusToString(solver.getModelStatus()),"objective":finite(solver.getObjectiveValue()),"wall_seconds":elapsed,"simplex_iterations":info.simplex_iteration_count,"mip_nodes":info.mip_node_count,"mip_gap":finite(info.mip_gap)}
def main()->int:
    p=argparse.ArgumentParser();p.add_argument("models",nargs="+",type=Path);p.add_argument("--module-path",type=Path);p.add_argument("--output",required=True,type=Path);a=p.parse_args();report={"schema_version":1,"platform":platform.platform(),"trust_boundary":"Optional reference process; never imported or linked by AstraNiti production targets","records":[run(m,a.module_path) for m in a.models]};a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2,allow_nan=False),encoding="utf-8");return 0 if all(x["executed"] for x in report["records"]) else 2
if __name__=="__main__":raise SystemExit(main())
