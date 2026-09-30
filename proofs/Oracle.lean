import Scalar
import Lean

/- Independent interpreter of the generated C kernel corpus, with exact
   BitVec arithmetic. This is differential testing, not a proof of the C port. -/
open Ziran
structure Term where
  op : Nat
  a : Int
  b : Int
  c : Int
  value : Nat

def number (s : String) : IO Nat :=
  match s.toNat? with
  | some n => pure n
  | none => throw (IO.userError s!"invalid natural: {s}")
def index (s : String) : IO Int :=
  match s.toInt? with
  | some n => pure n
  | none => throw (IO.userError s!"invalid index: {s}")

def eval (signed : Bool) (terms : Array Term) (x y : Nat) : Array Nat := Id.run do
  let mut values := #[]
  for t in terms do
    let get := fun (i : Int) => if i < 0 then 0 else values[i.toNat]!
    let a := get t.a
    let b := get t.b
    let c := get t.c
    let av := BitVec.ofNat 4 a
    let bv := BitVec.ofNat 4 b
    let value := match t.op with
      | 0 => t.value
      | 1 => if t.value == 0 then x else y
      | 2 => (av + bv).toNat
      | 3 => (av - bv).toNat
      | 4 => (av * bv).toNat
      | 5 => if a == b then 1 else 0
      | 6 => if orderValue signed av < orderValue signed bv then 1 else 0
      | 7 => if orderValue signed av ≤ orderValue signed bv then 1 else 0
      | 8 => if a != 0 && b != 0 then 1 else 0
      | 9 => if a != 0 || b != 0 then 1 else 0
      | 10 => if a == 0 then 1 else 0
      | 11 => if a != 0 then b else c
      | _ => 0
    values := values.push value
  return values

def main (args : List String) : IO Unit := do
  let [path] := args | throw (IO.userError "pass the kernel corpus")
  let lines := (← IO.FS.readFile path).splitOn "\n" |>.toArray
  let mut cursor := 0
  let mut checked := 0
  while cursor + 1 < lines.size do
    let ["G", sign, goal, count, accepted] := lines[cursor]!.splitOn " " |
      throw (IO.userError "invalid corpus header")
    let signed := (← number sign) == 1
    let goal ← number goal
    let count ← number count
    let accepted ← number accepted
    cursor := cursor + 1
    let mut terms := #[]
    for _ in [:count] do
      let [op, _type, a, b, c, value] := lines[cursor]!.splitOn " " |
        throw (IO.userError "invalid corpus term")
      terms := terms.push { op := ← number op, a := ← index a, b := ← index b,
                            c := ← index c, value := ← number value }
      cursor := cursor + 1
    if accepted == 1 then
      for x in [:16] do
        for y in [:16] do
          unless (eval signed terms x y)[goal]! == 1 do
            throw (IO.userError s!"C kernel accepted a false claim: {goal}, {x}, {y}")
      checked := checked + 1
  unless checked > 100 do throw (IO.userError "too few accepted claims")
  IO.println s!"Lean interpreter: checked {checked} accepted C claims over every 4-bit assignment"
