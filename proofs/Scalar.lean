import Std

/- Development model for the scalar proof rules. BitVec is arithmetic modulo
   2^width, including for signed words. Order uses the signed interpretation
   only when requested. This models rules, not the C implementation or compiler. -/
namespace Ziran

inductive Word (width : Nat) where
  | atom : Nat → Word width
  | literal : BitVec width → Word width
  | add : Word width → Word width → Word width
  | sub : Word width → Word width → Word width
  | mul : Word width → Word width → Word width
  deriving Repr

def Word.eval (ρ : Nat → BitVec w) : Word w → BitVec w
  | .atom n => ρ n
  | .literal v => v
  | .add a b => a.eval ρ + b.eval ρ
  | .sub a b => a.eval ρ - b.eval ρ
  | .mul a b => a.eval ρ * b.eval ρ

/- These are the identities used by polynomial normalization. A derivation is
   independent of variable values. There is no arbitrary equality constructor. -/
inductive Equation : Word w → Word w → Type where
  | refl a : Equation a a
  | symm : Equation a b → Equation b a
  | trans : Equation a b → Equation b c → Equation a c
  | addCong : Equation a b → Equation c d → Equation (.add a c) (.add b d)
  | subCong : Equation a b → Equation c d → Equation (.sub a c) (.sub b d)
  | mulCong : Equation a b → Equation c d → Equation (.mul a c) (.mul b d)
  | addComm a b : Equation (.add a b) (.add b a)
  | addAssoc a b c : Equation (.add (.add a b) c) (.add a (.add b c))
  | mulComm a b : Equation (.mul a b) (.mul b a)
  | mulAssoc a b c : Equation (.mul (.mul a b) c) (.mul a (.mul b c))
  | distribute a b c : Equation (.mul a (.add b c)) (.add (.mul a b) (.mul a c))
  | distributeRight a b c : Equation (.mul (.add a b) c) (.add (.mul a c) (.mul b c))
  | addZero a : Equation (.add a (.literal 0)) a
  | mulZero a : Equation (.mul a (.literal 0)) (.literal 0)
  | subAsAdd a b : Equation (.sub a b) (.add a (.sub (.literal 0) b))

theorem Equation.sound {a b : Word w} (h : Equation a b) (ρ : Nat → BitVec w) : a.eval ρ = b.eval ρ := by
  induction h with
  | refl => rfl
  | symm _ ih => exact ih.symm
  | trans _ _ ih₁ ih₂ => exact ih₁.trans ih₂
  | addCong _ _ ih₁ ih₂ => simp only [Word.eval, ih₁, ih₂]
  | subCong _ _ ih₁ ih₂ => simp only [Word.eval, ih₁, ih₂]
  | mulCong _ _ ih₁ ih₂ => simp only [Word.eval, ih₁, ih₂]
  | addComm => exact BitVec.add_comm _ _
  | addAssoc => exact BitVec.add_assoc _ _ _
  | mulComm => exact BitVec.mul_comm _ _
  | mulAssoc => exact BitVec.mul_assoc _ _ _
  | distribute => exact BitVec.mul_add
  | distributeRight => exact BitVec.add_mul
  | addZero => exact BitVec.add_zero _
  | mulZero => exact BitVec.mul_zero
  | subAsAdd => simp [Word.eval, BitVec.sub_eq_add_neg]

def orderValue (signed : Bool) (v : BitVec w) : Int :=
  if signed then v.toInt else Int.ofNat v.toNat

theorem orderValue_injective (s : Bool) {a b : BitVec w}
    (h : orderValue s a = orderValue s b) : a = b := by
  cases s
  · exact BitVec.eq_of_toNat_eq (Int.ofNat_inj.mp h)
  · exact BitVec.eq_of_toInt_eq h

inductive Formula (w : Nat) where
  | truth : Formula w
  | bit : Nat → Formula w
  | equal : Word w → Word w → Formula w
  | le : Word w → Word w → Formula w
  | lt : Word w → Word w → Formula w
  | neg : Formula w → Formula w
  | both : Formula w → Formula w → Formula w
  | either : Formula w → Formula w → Formula w

def Formula.eval (s : Bool) (ρ : Nat → BitVec w) (β : Nat → Bool) : Formula w → Prop
  | .truth => True
  | .bit n => β n = true
  | .equal a b => a.eval ρ = b.eval ρ
  | .le a b => orderValue s (a.eval ρ) ≤ orderValue s (b.eval ρ)
  | .lt a b => orderValue s (a.eval ρ) < orderValue s (b.eval ρ)
  | .neg p => ¬p.eval s ρ β
  | .both p q => p.eval s ρ β ∧ q.eval s ρ β
  | .either p q => p.eval s ρ β ∨ q.eval s ρ β

/- A derivation can use only its explicit branch hypotheses. The empty context
   is required for a top-level theorem or a lemma being used in another proof. -/
inductive Certificate : List (Formula w) → Formula w → Type where
  | hypothesis (p) : p ∈ Γ → Certificate Γ p
  | truth : Certificate Γ .truth
  | ring : Equation a b → Certificate Γ (.equal a b)
  | leRefl a : Certificate Γ (.le a a)
  | leTrans : Certificate Γ (.le a b) → Certificate Γ (.le b c) → Certificate Γ (.le a c)
  | ltTrans : Certificate Γ (.lt a b) → Certificate Γ (.lt b c) → Certificate Γ (.lt a c)
  | leLt : Certificate Γ (.le a b) → Certificate Γ (.lt b c) → Certificate Γ (.lt a c)
  | ltLe : Certificate Γ (.lt a b) → Certificate Γ (.le b c) → Certificate Γ (.lt a c)
  | antisym : Certificate Γ (.le a b) → Certificate Γ (.le b a) → Certificate Γ (.equal a b)
  | notLt : Certificate Γ (.neg (.lt a b)) → Certificate Γ (.le b a)
  | notLe : Certificate Γ (.neg (.le a b)) → Certificate Γ (.lt b a)
  | strictCycle : Certificate Γ (.lt a a) → Certificate Γ p
  | contradiction : Certificate Γ p → Certificate Γ (.neg p) → Certificate Γ q
  | both : Certificate Γ p → Certificate Γ q → Certificate Γ (.both p q)
  | left : Certificate Γ p → Certificate Γ (.either p q)
  | right : Certificate Γ q → Certificate Γ (.either p q)
  | cases p : Certificate (p :: Γ) q → Certificate (.neg p :: Γ) q → Certificate Γ q
  | use : Certificate [] p → Certificate Γ p

theorem Certificate.sound {Γ : List (Formula w)} {p : Formula w} (h : Certificate Γ p) (s : Bool)
    (ρ : Nat → BitVec w) (β : Nat → Bool)
    (assumptions : ∀ f ∈ Γ, f.eval s ρ β) : p.eval s ρ β := by
  induction h with
  | hypothesis p member => exact assumptions p member
  | truth => trivial
  | ring eq => exact eq.sound ρ
  | leRefl => exact Int.le_refl _
  | leTrans _ _ ih₁ ih₂ => exact Int.le_trans (ih₁ assumptions) (ih₂ assumptions)
  | ltTrans _ _ ih₁ ih₂ => exact Int.lt_trans (ih₁ assumptions) (ih₂ assumptions)
  | leLt _ _ ih₁ ih₂ => exact Int.lt_of_le_of_lt (ih₁ assumptions) (ih₂ assumptions)
  | ltLe _ _ ih₁ ih₂ => exact Int.lt_of_lt_of_le (ih₁ assumptions) (ih₂ assumptions)
  | antisym _ _ ih₁ ih₂ => exact orderValue_injective s (Int.le_antisymm (ih₁ assumptions) (ih₂ assumptions))
  | notLt _ ih => exact Int.not_lt.mp (ih assumptions)
  | notLe _ ih => exact Int.not_le.mp (ih assumptions)
  | strictCycle _ ih => exact False.elim (Int.lt_irrefl _ (ih assumptions))
  | contradiction _ _ ih₁ ih₂ => exact False.elim (ih₂ assumptions (ih₁ assumptions))
  | both _ _ ih₁ ih₂ => exact ⟨ih₁ assumptions, ih₂ assumptions⟩
  | left _ ih => exact Or.inl (ih assumptions)
  | right _ ih => exact Or.inr (ih assumptions)
  | cases p _ _ ih₁ ih₂ =>
      by_cases hp : p.eval s ρ β
      · apply ih₁
        intro f member
        rcases List.mem_cons.mp member with rfl | member
        · exact hp
        · exact assumptions f member
      · apply ih₂
        intro f member
        rcases List.mem_cons.mp member with rfl | member
        · exact hp
        · exact assumptions f member
  | use _ ih => exact ih (by simp)

theorem theorem_sound {p : Formula w} (h : Certificate [] p) :
    ∀ s ρ β, p.eval s ρ β := fun s ρ β => h.sound s ρ β (by simp)

/- Unfolding an if is an ITE. These justify branch reduction and congruent
   rewriting even when the rewritten word is inside a boolean predicate. -/
theorem branch_true (p : Bool) (h : p = true) (a b : α) : (if p then a else b) = a := by simp [h]
theorem branch_false (p : Bool) (h : p = false) (a b : α) : (if p then a else b) = b := by simp [h]
theorem branch_equal (p : Bool) (a : α) : (if p then a else a) = a := by cases p <;> rfl
theorem rewrite_sound {a b : α} (h : a = b) (predicate : α → Prop) (proof : predicate b) : predicate a := by
  cases h
  exact proof

/- Checked procedure projection remains a trust boundary. The model starts
   with the scalar expression after projection; it does not verify lowering. -/
#print axioms Equation.sound
#print axioms theorem_sound
#print axioms rewrite_sound

end Ziran
