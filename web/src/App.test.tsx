import { render, screen } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { describe, expect, it, vi } from 'vitest'
import { AiProposal, ServiceState } from './components'

describe('AI formulation safety',()=>{
  it('requires review before the load confirmation becomes available',async()=>{
    const load=vi.fn(); const user=userEvent.setup()
    render(<AiProposal proposal={{summary:'Binary allocation',formulation:'max 4 x',warnings:[]}} onLoad={load}/>)
    expect(screen.getByRole('button',{name:/load formulation/i})).toBeDisabled()
    await user.click(screen.getByRole('checkbox',{name:/reviewed/i}))
    await user.click(screen.getByRole('button',{name:/load formulation/i}))
    expect(screen.getByRole('dialog')).toBeInTheDocument()
    expect(load).not.toHaveBeenCalled()
    await user.click(screen.getByRole('button',{name:/confirm load/i}))
    expect(load).toHaveBeenCalledOnce()
  })
})

describe('service availability',()=>{
  it('explains unavailable GPU without presenting fabricated metrics',()=>{
    render(<ServiceState service="GPU" status="unavailable" detail="No compatible CUDA device detected"/>)
    expect(screen.getByText(/No compatible CUDA device detected/)).toBeInTheDocument()
    expect(screen.getByText('UNAVAILABLE')).toBeInTheDocument()
  })
})
