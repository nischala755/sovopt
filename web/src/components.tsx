import { useState, type ReactNode } from 'react'
import type { AiProposalData, ServiceStatus, TelemetryPoint } from './api'

export function ServiceState({service,status,detail}:{service:string;status:ServiceStatus;detail?:string}){
 return <div className={`service service-${status}`} role="status"><span className="signal"/><div><strong>{service}</strong><small>{detail||status}</small></div><b>{status.toUpperCase()}</b></div>
}
export function Panel({title,kicker,actions,children,className=''}:{title:string;kicker?:string;actions?:ReactNode;children:ReactNode;className?:string}){
 return <section className={`panel ${className}`}><header><div>{kicker&&<p className="kicker">{kicker}</p>}<h2>{title}</h2></div>{actions}</header>{children}</section>
}
export function EmptyState({title,detail}:{title:string;detail:string}){return <div className="empty"><span>◇</span><strong>{title}</strong><p>{detail}</p></div>}
export function Metric({label,value,unit}:{label:string;value:string|number|null|undefined;unit?:string}){return <div className="metric"><small>{label}</small><strong>{value??'—'}</strong>{value!=null&&unit&&<span>{unit}</span>}</div>}

const colors=['#ffb84d','#39d9b6','#7e9cff','#f05d7a','#b887ff']
export function TelemetryChart({points,series}:{points:TelemetryPoint[];series:{key:keyof TelemetryPoint;label:string}[]}){
 if(!points.length)return <EmptyState title="No telemetry yet" detail="Start a solve to stream measured solver events."/>
 const W=760,H=230,pad=30, vals=series.flatMap(s=>points.map(p=>Number(p[s.key])).filter(Number.isFinite));const lo=Math.min(...vals),hi=Math.max(...vals);const x=(i:number)=>pad+(i/Math.max(1,points.length-1))*(W-pad*2);const y=(v:number)=>H-pad-((v-lo)/Math.max(1e-9,hi-lo))*(H-pad*2)
 return <div className="chart"><svg viewBox={`0 0 ${W} ${H}`} role="img" aria-label={`${series.map(s=>s.label).join(', ')} telemetry chart`}><path className="grid" d={`M${pad} ${pad}V${H-pad}H${W-pad}`}/>{series.map((s,j)=>{const valid=points.map((p,i)=>({v:Number(p[s.key]),i})).filter(p=>Number.isFinite(p.v));return <polyline key={String(s.key)} fill="none" stroke={colors[j]} strokeWidth="2.5" points={valid.map(p=>`${x(p.i)},${y(p.v)}`).join(' ')}/>})}</svg><div className="legend">{series.map((s,j)=><span key={String(s.key)}><i style={{background:colors[j]}}/>{s.label}</span>)}</div></div>
}
export function AiProposal({proposal,onLoad}:{proposal:AiProposalData;onLoad:()=>void}){
 const [reviewed,setReviewed]=useState(false),[confirm,setConfirm]=useState(false)
 return <article className="proposal"><div className="proposal-label">AI-PROPOSED · UNVERIFIED</div><h3>{proposal.summary}</h3><pre>{proposal.formulation}</pre>{proposal.warnings.length>0&&<ul>{proposal.warnings.map(w=><li key={w}>{w}</li>)}</ul>}<label className="review"><input type="checkbox" checked={reviewed} onChange={e=>setReviewed(e.target.checked)}/> I have reviewed variables, domains, constraints, and objective</label><button disabled={!reviewed} onClick={()=>setConfirm(true)}>Review and Load Formulation</button>{confirm&&<div className="modal-backdrop"><div role="dialog" aria-modal="true" aria-labelledby="confirm-title" className="modal"><p className="kicker">EXPLICIT CONFIRMATION</p><h2 id="confirm-title">Load AI-proposed formulation?</h2><p>This replaces the current draft. The proposal is non-authoritative and must be independently verified.</p><div className="row"><button className="ghost" onClick={()=>setConfirm(false)}>Cancel</button><button onClick={()=>{onLoad();setConfirm(false)}}>Confirm Load</button></div></div></div>}</article>
}
