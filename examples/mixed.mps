* Domain representation only: no integer optimization is performed.
* Hand-counted: 3 variables, 2 rows, 5 matrix nonzeros; 2 integer (1 binary).
NAME MIXED
ROWS
 N COST
 G DEMAND
 E BALANCE
COLUMNS
 flow COST 2 DEMAND 1
 flow BALANCE 1
 mark0 'MARKER' 'INTORG'
 trucks COST 7 DEMAND 4
 trucks BALANCE -1
 open COST 3 DEMAND 2
 mark1 'MARKER' 'INTEND'
RHS
 rhs DEMAND 5 BALANCE 0
BOUNDS
 UP b flow 10
 UP b trucks 3
 BV b open
ENDATA
