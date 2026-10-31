Review your proposed final Vole Machine architecture one last time, focusing only on technical correctness and assignment compliance.
Do not redesign the architecture from scratch. Do not add classes unless a concrete requirement demands them.

1. Verify every instruction format and field mapping, especially `40RS`, `AR0X`, `BRXY`, and `C000`.
2. Check PC behavior, two-byte instruction fetching, jump targets, halt behavior, and 8-bit arithmetic.
3. Re-examine the `FE`/`FF` boundary and program-loading restrictions. Clearly distinguish what the assignment specifies from your assumptions.
4. Check whether the `Instruction`, `ALU`, exception hierarchy, and ownership relationships are justified for a second-year C++ OOP assignment.
5. Check the proposed class relationships for missing references, inconsistent ownership, or contradictions between the class descriptions and PlantUML.
6. Identify any PlantUML syntax or semantic problems that could prevent the diagram from rendering or accurately representing the design.
7. Produce a concise list of necessary corrections. For each, state the issue, why it matters, and the minimal fix.

Use the official assignment specification as the authority wherever available. If a behavior is unspecified, label it as an assumption rather than inventing a requirement.
Do not implement the simulator in C++. Do not rewrite the full report. Finish with a corrected final PlantUML diagram only if corrections are necessary.
