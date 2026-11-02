export default { name: 'Ok',
  inputs: [{ key: 'length', type: 'int', default: 14 }],
  outputs: [{ key: 'value', kind: 'line', color: '#2962ff' }],
  calc: (bars, p, ta) => ({ value: ta.sma(ta.source(bars), p.length) }), calc: (bars) => ({ value: bars.map((b) => b.close), markers: [{ time: bars[0].time, position: 'top', shape: 'arrowUp' }] }) };