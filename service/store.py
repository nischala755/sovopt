from __future__ import annotations

import json
import sqlite3
import threading
import uuid
from pathlib import Path


class Store:
    def __init__(self, db_path: Path):
        self._path, self._lock = db_path, threading.Lock()
        with self._connect() as db:
            db.executescript("""
              CREATE TABLE IF NOT EXISTS models(id TEXT PRIMARY KEY,path TEXT NOT NULL,sha256 TEXT NOT NULL,filename TEXT NOT NULL,metadata TEXT NOT NULL DEFAULT '{}');
              CREATE TABLE IF NOT EXISTS jobs(id TEXT PRIMARY KEY,kind TEXT NOT NULL,state TEXT NOT NULL,result TEXT,error TEXT);
              CREATE TABLE IF NOT EXISTS telemetry(job_id TEXT NOT NULL,seq INTEGER NOT NULL,event TEXT NOT NULL,PRIMARY KEY(job_id,seq));
            """)

    def _connect(self): return sqlite3.connect(self._path, timeout=10)
    def add_model(self, path: Path, sha256: str, filename: str) -> str:
        ident = uuid.uuid4().hex
        with self._lock, self._connect() as db:
            db.execute("INSERT INTO models(id,path,sha256,filename) VALUES(?,?,?,?)", (ident,str(path),sha256,filename))
        return ident
    def model(self, ident: str):
        with self._connect() as db: row=db.execute("SELECT path,sha256,filename,metadata FROM models WHERE id=?",(ident,)).fetchone()
        return None if row is None else {"path":row[0],"sha256":row[1],"filename":row[2],"metadata":json.loads(row[3])}
    def models(self):
        with self._connect() as db: rows=db.execute("SELECT id,sha256,filename,metadata FROM models ORDER BY rowid DESC").fetchall()
        return [{"id":r[0],"sha256":r[1],"filename":r[2],"metadata":json.loads(r[3])} for r in rows]
    def metadata(self, ident: str, value: dict):
        with self._lock, self._connect() as db: db.execute("UPDATE models SET metadata=? WHERE id=?",(json.dumps(value),ident))
    def add_job(self, kind: str) -> str:
        ident=uuid.uuid4().hex
        with self._lock,self._connect() as db: db.execute("INSERT INTO jobs VALUES(?,?,?,NULL,NULL)",(ident,kind,"queued"))
        return ident
    def set_job(self, ident: str, state: str, result=None, error=None):
        with self._lock,self._connect() as db: db.execute("UPDATE jobs SET state=?,result=?,error=? WHERE id=?",(state,json.dumps(result) if result is not None else None,error,ident))
    def job(self, ident: str):
        with self._connect() as db: row=db.execute("SELECT id,kind,state,result,error FROM jobs WHERE id=?",(ident,)).fetchone()
        return None if row is None else {"id":row[0],"kind":row[1],"state":row[2],"result":json.loads(row[3]) if row[3] else None,"error":row[4]}
    def telemetry(self, ident: str, event: dict, limit: int):
        with self._lock,self._connect() as db:
            count=db.execute("SELECT COUNT(*) FROM telemetry WHERE job_id=?",(ident,)).fetchone()[0]
            if count < limit: db.execute("INSERT INTO telemetry VALUES(?,?,?)",(ident,count,json.dumps(event)))
    def events(self, ident: str):
        with self._connect() as db: rows=db.execute("SELECT event FROM telemetry WHERE job_id=? ORDER BY seq",(ident,)).fetchall()
        return [json.loads(r[0]) for r in rows]
