export type ServiceStatus='available'|'degraded'|'unavailable'|'checking'
export type TelemetryPoint={elapsed:number;gap?:number|null;incumbent?:number|null;bound?:number|null;nodes?:number|null;cpu?:number|null;gpu?:number|null}
export type Job={id:string;status:'queued'|'running'|'completed'|'failed';progress?:number;message?:string;result?:Record<string,unknown>;telemetry?:TelemetryPoint[]}
export type ModelInfo={id:string;name:string;format?:string;variables?:number;constraints?:number;nonzeros?:number;integer_variables?:number;sense?:string;fingerprint?:string}
export type BenchmarkResult={backend:'cpu'|'gpu'|'adaptive';status:string;wall_seconds?:number;objective?:number;gap?:number;nodes?:number;detail?:string}
export type AiProposalData={summary:string;formulation:string;warnings:string[];confirmation_token?:string}

const base=(import.meta.env.VITE_API_BASE_URL||'/api').replace(/\/$/,'')
async function request<T>(path:string,init?:RequestInit):Promise<T>{
 const response=await fetch(`${base}${path}`,{...init,headers:{...(init?.body instanceof FormData?{}:{'Content-Type':'application/json'}),...init?.headers}})
 if(!response.ok){let detail=`Request failed (${response.status})`;try{const body=await response.json() as {detail?:string};if(body.detail)detail=body.detail}catch{}throw new Error(detail)}
 return response.json() as Promise<T>
}
type RawJob={id?:string;job_id?:string;state:string;result?:Record<string,unknown>;error?:string}
const adaptJob=(raw:RawJob):Job=>({id:raw.id||raw.job_id||'',status:raw.state==='succeeded'?'completed':raw.state as Job['status'],result:raw.result,message:raw.error})
const num=(v:unknown)=>typeof v==='number'&&Number.isFinite(v)?v:null
const adaptTelemetry=(events:Array<Record<string,unknown>>):TelemetryPoint[]=>events.map(e=>({elapsed:Number(e.elapsed_seconds||0),gap:num(e.mip_gap),incumbent:num(e.objective),bound:num(e.best_bound),nodes:num(e.nodes),cpu:num(e.cpu_seconds),gpu:num(e.gpu_kernel_seconds)}))

export const api={
 health:()=>request<Record<string,ServiceStatus|boolean|string|null>>('/health'),
 models:()=>request<ModelInfo[]>('/models'), model:(id:string)=>request<ModelInfo>(`/models/${id}`),
 upload:async(file:File)=>{const uploaded=await request<{model_id:string;sha256:string}>('/models/upload',{method:'POST',body:await file.arrayBuffer(),headers:{'Content-Type':'application/octet-stream','X-Filename':file.name}});const details=await request<Record<string,unknown>>(`/models/${uploaded.model_id}/inspect`);return {id:uploaded.model_id,name:file.name,fingerprint:uploaded.sha256,...details} as ModelInfo},
 fingerprint:(id:string)=>request<Record<string,unknown>>(`/models/${id}/fingerprint`),
 solve:async(id:string,options:Record<string,unknown>)=>{const kind=String(options.kind||'lp');const solverOptions={...options};delete solverOptions.kind;delete solverOptions.backend;return adaptJob(await request<RawJob>(`/models/${id}/solve`,{method:'POST',body:JSON.stringify({kind,options:solverOptions})}))},
 job:async(id:string)=>adaptJob(await request<RawJob>(`/jobs/${id}`)),
 telemetry:async(id:string)=>adaptTelemetry((await request<{events:Array<Record<string,unknown>>}>(`/jobs/${id}/telemetry`)).events),
 benchmark:async(modelId:string,backends:string[])=>adaptJob(await request<RawJob>(`/models/${modelId}/execution-benchmark`,{method:'POST',body:JSON.stringify({backends,repetitions:3})})),
 benchmarkResults:async(id:string)=>{const raw=await request<RawJob>(`/jobs/${id}`);const groups=(raw.result||[]) as unknown as Array<{backend:BenchmarkResult['backend'];records:Array<Record<string,unknown>>}>;return groups.map(g=>{const r=g.records[0]||{};return {backend:g.backend,status:String(r.status||'unavailable'),wall_seconds:num(r.wall_seconds)??undefined,detail:String(r.detail||'')} as BenchmarkResult})},
 explain:async(modelId:string)=>{const x=await request<{available:boolean;explanation:string|null}>('/ai/explain',{method:'POST',body:JSON.stringify({model_id:modelId})});return {text:x.explanation||'AI explanation unavailable'}},
 formulate:async(prompt:string)=>{const x=await request<{available:boolean;proposal?:Record<string,unknown>;confirmation_token?:string}>('/ai/formulations/propose',{method:'POST',body:JSON.stringify({prompt})});return {summary:String(x.proposal?.name||'Structured formulation proposal'),formulation:JSON.stringify(x.proposal,null,2),warnings:['Review every coefficient and bound before confirmation.'],confirmation_token:x.confirmation_token}},
 confirm:(token:string)=>request<Record<string,unknown>>('/ai/formulations/confirm',{method:'POST',body:JSON.stringify({confirmation_token:token})})
}
export async function pollJob(id:string,onUpdate:(job:Job)=>void,signal?:AbortSignal){while(!signal?.aborted){const current=await api.job(id);onUpdate(current);if(['completed','failed'].includes(current.status))return current;await new Promise(r=>setTimeout(r,1000))}}
