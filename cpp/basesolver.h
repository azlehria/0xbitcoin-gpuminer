#ifndef _BASESOLVER_H_
#define _BASESOLVER_H_

class IBaseSolver
{
public:
  virtual ~IBaseSolver() {};

  auto virtual findSolution() -> void = 0;
  auto virtual stopFinding() -> void = 0;

  auto virtual getHashrate() const -> double const = 0;

  auto virtual updateTarget() -> void = 0;
  auto virtual updateMessage() -> void = 0;
};

#endif // !_BASESOLVER_H_
