using UnityEngine;

public class SmartChaseSimulation : MonoBehaviour
{
    [Header("Objects")]
    public Transform killer;
    public Transform runner;

    [Header("Arena")]
    public Vector2 arenaSize = new Vector2(8.5f, 4.5f);
    public float catchDistance = 0.42f;

    [Header("Goal")]
    public float baseKillerSpeed = 1.55f;
    public float baseRunnerSpeed = 1.75f;
    public float strategyGrowth = 0.04f;
    public float maxStrategy = 1f;

    readonly float[] speedOptions = { 1f, 2f, 5f, 10f, 50f, 100f, 250f, 500f, 1000f };
    int round = 1;
    int killerScore;
    float killerStrategy = 0.25f;
    float runnerStrategy = 0.25f;
    float killerChaseWeight = 1.2f;
    float killerPredictWeight = 0.2f;
    float runnerDistanceWeight = 1.2f;
    float runnerWallWeight = 0.5f;
    float runnerMemoryWeight = 0.3f;
    Vector2 killerLastChoice;
    Vector2 runnerLastChoice;
    const int StateCount = 32;
    const int ActionCount = 8;
    readonly Vector2[] actions =
    {
        Vector2.up,
        Vector2.down,
        Vector2.left,
        Vector2.right,
        new Vector2(1f, 1f).normalized,
        new Vector2(-1f, 1f).normalized,
        new Vector2(1f, -1f).normalized,
        new Vector2(-1f, -1f).normalized
    };
    readonly float[,] killerQ = new float[StateCount, ActionCount];
    readonly float[,] runnerQ = new float[StateCount, ActionCount];
    float learningRate = 0.18f;
    float discount = 0.85f;
    float exploration = 0.12f;
    int killerLastState = -1;
    int killerLastAction = -1;
    int runnerLastState = -1;
    int runnerLastAction = -1;
    float roundTimer;
    float bestSurvivalTime;
    Vector2 killerVelocity;
    Vector2 runnerVelocity;
    Vector2 runnerTarget;
    Vector2 lastCatchPosition = new Vector2(99f, 99f);
    float runnerDecisionTimer;
    int speedIndex;
    Rect[] wallRects;

    void Awake()
    {
        wallRects = new Rect[]
        {
            CenterRect(new Vector2(-2.4f, 1.35f), new Vector2(0.5f, 2.2f)),
            CenterRect(new Vector2(1.4f, -1.15f), new Vector2(0.5f, 2.4f)),
            CenterRect(new Vector2(3.9f, 1.55f), new Vector2(2.0f, 0.45f)),
            CenterRect(new Vector2(-4.6f, -1.55f), new Vector2(2.2f, 0.45f))
        };

        EnsureWorld();
        ResetRound();
    }

    void OnDestroy()
    {
        Time.timeScale = 1f;
    }

    void Update()
    {
        if (killer == null || runner == null)
        {
            return;
        }

        float remainingTime = Time.unscaledDeltaTime * speedOptions[speedIndex];
        float maxStep = 0.02f;

        while (remainingTime > 0f)
        {
            float step = Mathf.Min(maxStep, remainingTime);
            Simulate(step);
            remainingTime -= step;
        }
    }

    void Simulate(float deltaTime)
    {
        float oldDistance = Vector2.Distance(killer.position, runner.position);
        MoveRunner(deltaTime);
        MoveKiller(deltaTime);
        UpdateSurvivalTime(deltaTime);
        LearnStep(oldDistance);

        if (Vector2.Distance(killer.position, runner.position) <= catchDistance)
        {
            EndRound();
        }
    }

    void UpdateSurvivalTime(float deltaTime)
    {
        roundTimer += deltaTime;
        bestSurvivalTime = Mathf.Max(bestSurvivalTime, roundTimer);
    }

    void EndRound()
    {
        killerScore++;
        lastCatchPosition = runner.position;

        float fastCatch = Mathf.InverseLerp(12f, 2f, roundTimer);
        float longSurvival = Mathf.InverseLerp(4f, 20f, roundTimer);
        killerStrategy = Mathf.Min(maxStrategy, killerStrategy + strategyGrowth * (0.5f + fastCatch));
        runnerStrategy = Mathf.Min(maxStrategy, runnerStrategy + strategyGrowth * (0.5f + longSurvival));
        LearnFromRound(fastCatch, longSurvival);
        ReinforceLastActions(8f, -8f);
        exploration = Mathf.Max(0.02f, exploration * 0.99f);
        round++;
        ResetRound();
    }

    void LearnStep(float oldDistance)
    {
        float newDistance = Vector2.Distance(killer.position, runner.position);
        float distanceChange = oldDistance - newDistance;
        ReinforceLastActions(distanceChange * 0.8f, -distanceChange * 0.8f + 0.02f);
    }

    void ReinforceLastActions(float killerReward, float runnerReward)
    {
        if (killerLastState >= 0 && killerLastAction >= 0)
        {
            UpdateQ(killerQ, killerLastState, killerLastAction, killerReward, GetKillerState());
        }

        if (runnerLastState >= 0 && runnerLastAction >= 0)
        {
            UpdateQ(runnerQ, runnerLastState, runnerLastAction, runnerReward, GetRunnerState());
        }
    }

    void UpdateQ(float[,] table, int state, int action, float reward, int nextState)
    {
        float oldValue = table[state, action];
        float learnedValue = reward + discount * BestQ(table, nextState);
        table[state, action] = Mathf.Lerp(oldValue, learnedValue, learningRate);
    }

    float BestQ(float[,] table, int state)
    {
        float best = table[state, 0];
        for (int i = 1; i < ActionCount; i++)
        {
            best = Mathf.Max(best, table[state, i]);
        }

        return best;
    }

    void LearnFromRound(float fastCatch, float longSurvival)
    {
        killerChaseWeight = Mathf.Clamp(killerChaseWeight + 0.08f * fastCatch - 0.02f * longSurvival, 0.4f, 3f);
        killerPredictWeight = Mathf.Clamp(killerPredictWeight + 0.12f * fastCatch + 0.03f, 0f, 3f);
        runnerDistanceWeight = Mathf.Clamp(runnerDistanceWeight + 0.1f * longSurvival + 0.02f, 0.4f, 3f);
        runnerWallWeight = Mathf.Clamp(runnerWallWeight + 0.08f * longSurvival, 0f, 3f);
        runnerMemoryWeight = Mathf.Clamp(runnerMemoryWeight + 0.12f * fastCatch, 0f, 3f);
    }

    void HandleFastForwardKey(Event guiEvent)
    {
        if (guiEvent.type != EventType.KeyDown)
        {
            return;
        }

        if (guiEvent.keyCode == KeyCode.Equals || guiEvent.keyCode == KeyCode.Plus || guiEvent.keyCode == KeyCode.RightBracket)
        {
            SetSpeedIndex(speedIndex + 1);
            guiEvent.Use();
        }

        if (guiEvent.keyCode == KeyCode.Minus || guiEvent.keyCode == KeyCode.LeftBracket)
        {
            SetSpeedIndex(speedIndex - 1);
            guiEvent.Use();
        }

        if (guiEvent.keyCode == KeyCode.Alpha0)
        {
            SetSpeedIndex(0);
            guiEvent.Use();
        }
    }

    void SetSpeedIndex(int nextIndex)
    {
        speedIndex = Mathf.Clamp(nextIndex, 0, speedOptions.Length - 1);
    }

    void MoveRunner(float deltaTime)
    {
        runnerDecisionTimer -= deltaTime;
        if (runnerDecisionTimer <= 0f)
        {
            runnerDecisionTimer = Mathf.Lerp(0.65f, 0.12f, runnerStrategy);
            runnerTarget = PickRunnerTarget();
        }

        Vector2 position = runner.position;
        Vector2 direction = PickRunnerMove(position);

        runnerVelocity = direction * (baseRunnerSpeed + runnerStrategy * 0.25f);
        runner.position = MoveWithWalls(position, runnerVelocity * deltaTime);
    }

    void MoveKiller(float deltaTime)
    {
        Vector2 killerPosition = killer.position;
        Vector2 direction = PickKillerMove(killerPosition);

        killerVelocity = direction * (baseKillerSpeed + killerStrategy * 0.35f);
        Vector2 nextPosition = MoveWithWalls(killerPosition, killerVelocity * deltaTime);
        if (nextPosition == killerPosition)
        {
            Vector2 directDirection = ((Vector2)runner.position - killerPosition).normalized;
            Vector2 sideStep = new Vector2(-directDirection.y, directDirection.x);
            if (Vector2.Dot(sideStep, runnerVelocity) < 0f)
            {
                sideStep = -sideStep;
            }

            nextPosition = MoveWithWalls(killerPosition, sideStep * killerVelocity.magnitude * deltaTime);
        }

        killer.position = nextPosition;
    }

    Vector2 PickRunnerMove(Vector2 position)
    {
        int state = GetRunnerState();
        int learnedAction = PickAction(runnerQ, state);
        Vector2 bestDirection = actions[learnedAction];
        float bestScore = float.MinValue;
        for (int i = 0; i < actions.Length; i++)
        {
            Vector2 candidate = MoveWithWalls(position, actions[i] * 0.75f);
            float moved = Vector2.Distance(candidate, position);
            float score =
                Vector2.Distance(candidate, killer.position) * runnerDistanceWeight * 2f
                + Vector2.Dot(actions[i], ((Vector2)position - (Vector2)killer.position).normalized) * 1.4f
                + moved * 0.8f
                - WallDanger(candidate) * runnerWallWeight * 2f
                - Mathf.Max(0f, 3.5f - Vector2.Distance(candidate, lastCatchPosition)) * runnerMemoryWeight
                - Mathf.Max(Mathf.Abs(candidate.x) / arenaSize.x, Mathf.Abs(candidate.y) / arenaSize.y) * 1.4f
                + runnerQ[state, i] * 0.6f;

            if (moved < 0.05f)
            {
                score -= 6f;
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestDirection = actions[i];
                learnedAction = i;
            }
        }

        runnerLastState = state;
        runnerLastAction = learnedAction;
        runnerLastChoice = bestDirection;
        return bestDirection.normalized;
    }

    Vector2 PickKillerMove(Vector2 position)
    {
        int state = GetKillerState();
        int learnedAction = PickAction(killerQ, state);
        Vector2 runnerPosition = runner.position;
        Vector2 predictedRunnerPosition = runnerPosition + runnerVelocity * Mathf.Lerp(0.1f, 1.4f, killerStrategy);
        Vector2 bestDirection = actions[learnedAction];
        float bestScore = float.MinValue;

        for (int i = 0; i < actions.Length; i++)
        {
            Vector2 candidate = MoveWithWalls(position, actions[i] * 0.75f);
            float moved = Vector2.Distance(candidate, position);
            float score =
                -Vector2.Distance(candidate, runnerPosition) * killerChaseWeight * 2.2f
                -Vector2.Distance(candidate, predictedRunnerPosition) * killerPredictWeight
                + Vector2.Dot(actions[i], (runnerPosition - position).normalized) * 1.6f
                + moved * 0.6f
                + killerQ[state, i] * 0.6f;

            if (PathCrossesWall(position, candidate))
            {
                score -= 8f;
            }

            if (moved < 0.05f)
            {
                score -= 6f;
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestDirection = actions[i];
                learnedAction = i;
            }
        }

        killerLastState = state;
        killerLastAction = learnedAction;
        killerLastChoice = bestDirection;
        return bestDirection.normalized;
    }

    int PickAction(float[,] table, int state)
    {
        if (Random.value < exploration)
        {
            return Random.Range(0, ActionCount);
        }

        int bestAction = 0;
        float bestValue = table[state, 0];
        for (int i = 1; i < ActionCount; i++)
        {
            if (table[state, i] > bestValue)
            {
                bestValue = table[state, i];
                bestAction = i;
            }
        }

        return bestAction;
    }

    int GetKillerState()
    {
        return GetState(killer.position, runner.position);
    }

    int GetRunnerState()
    {
        return GetState(runner.position, killer.position);
    }

    int GetState(Vector2 self, Vector2 other)
    {
        Vector2 relative = other - self;
        float angle = Mathf.Atan2(relative.y, relative.x) * Mathf.Rad2Deg;
        if (angle < 0f)
        {
            angle += 360f;
        }

        int directionBucket = Mathf.Clamp(Mathf.FloorToInt(angle / 45f), 0, 7);
        bool nearWall = WallDanger(self) > 0.4f;
        bool nearLastCatch = Vector2.Distance(self, lastCatchPosition) < 2.5f;
        return directionBucket + (nearWall ? 8 : 0) + (nearLastCatch ? 16 : 0);
    }

    Vector2 PickRunnerTarget()
    {
        Vector2 killerPosition = killer.position;
        Vector2 best = Vector2.zero;
        float bestScore = float.MinValue;

        int candidateCount = Mathf.RoundToInt(Mathf.Lerp(6f, 24f, runnerStrategy));
        for (int i = 0; i < candidateCount; i++)
        {
            Vector2 candidate = new Vector2(
                Random.Range(-arenaSize.x, arenaSize.x),
                Random.Range(-arenaSize.y, arenaSize.y)
            );

            float distanceFromKiller = Vector2.Distance(candidate, killerPosition);
            float edgePenalty = Mathf.Max(Mathf.Abs(candidate.x) / arenaSize.x, Mathf.Abs(candidate.y) / arenaSize.y);
            float catchMemoryPenalty = Mathf.Max(0f, 3.5f - Vector2.Distance(candidate, lastCatchPosition)) * runnerStrategy;
            float wallDanger = WallDanger(candidate) * runnerStrategy;
            float score = distanceFromKiller - edgePenalty * (1.2f + runnerStrategy * 0.5f) - catchMemoryPenalty - wallDanger;
            if (IsInsideWall(candidate))
            {
                score -= 100f;
            }

            if (score > bestScore)
            {
                bestScore = score;
                best = candidate;
            }
        }

        return best;
    }

    Vector2 ClampToArena(Vector2 position)
    {
        position.x = Mathf.Clamp(position.x, -arenaSize.x, arenaSize.x);
        position.y = Mathf.Clamp(position.y, -arenaSize.y, arenaSize.y);
        return position;
    }

    Vector2 MoveWithWalls(Vector2 position, Vector2 delta)
    {
        Vector2 next = ClampToArena(position + delta);
        if (!IsInsideWall(next))
        {
            return next;
        }

        Vector2 slideX = ClampToArena(position + new Vector2(delta.x, 0f));
        if (!IsInsideWall(slideX))
        {
            return slideX;
        }

        Vector2 slideY = ClampToArena(position + new Vector2(0f, delta.y));
        if (!IsInsideWall(slideY))
        {
            return slideY;
        }

        return position;
    }

    bool IsInsideWall(Vector2 position)
    {
        if (wallRects == null)
        {
            return false;
        }

        for (int i = 0; i < wallRects.Length; i++)
        {
            if (wallRects[i].Contains(position))
            {
                return true;
            }
        }

        return false;
    }

    bool PathCrossesWall(Vector2 start, Vector2 end)
    {
        int checks = 8;
        for (int i = 1; i <= checks; i++)
        {
            Vector2 point = Vector2.Lerp(start, end, i / (float)checks);
            if (IsInsideWall(point))
            {
                return true;
            }
        }

        return false;
    }

    float WallDanger(Vector2 position)
    {
        if (wallRects == null)
        {
            return 0f;
        }

        float danger = 0f;
        for (int i = 0; i < wallRects.Length; i++)
        {
            Rect wall = wallRects[i];
            float closestX = Mathf.Clamp(position.x, wall.xMin, wall.xMax);
            float closestY = Mathf.Clamp(position.y, wall.yMin, wall.yMax);
            float distance = Vector2.Distance(position, new Vector2(closestX, closestY));
            danger += Mathf.Max(0f, 1.4f - distance);
        }

        return danger;
    }

    Rect CenterRect(Vector2 center, Vector2 size)
    {
        return new Rect(center - size * 0.5f, size);
    }

    void ResetRound()
    {
        killer.position = new Vector3(-arenaSize.x * 0.75f, 0f, 0f);
        runner.position = new Vector3(arenaSize.x * 0.75f, 0f, 0f);
        killerVelocity = Vector2.zero;
        runnerVelocity = Vector2.zero;
        runnerDecisionTimer = 0f;
        runnerTarget = runner.position;
        roundTimer = 0f;
    }

    void EnsureWorld()
    {
        killer = killer != null ? killer : CreateBox("Killer", new Color(0.9f, 0.08f, 0.08f), new Vector2(-3f, 0f));
        runner = runner != null ? runner : CreateBox("Runner", new Color(0.1f, 0.85f, 0.25f), new Vector2(3f, 0f));

        CreateWall("Top Wall", new Vector2(0f, arenaSize.y + 0.25f), new Vector2(arenaSize.x * 2f + 1f, 0.25f));
        CreateWall("Bottom Wall", new Vector2(0f, -arenaSize.y - 0.25f), new Vector2(arenaSize.x * 2f + 1f, 0.25f));
        CreateWall("Left Wall", new Vector2(-arenaSize.x - 0.25f, 0f), new Vector2(0.25f, arenaSize.y * 2f + 1f));
        CreateWall("Right Wall", new Vector2(arenaSize.x + 0.25f, 0f), new Vector2(0.25f, arenaSize.y * 2f + 1f));

        if (wallRects != null)
        {
            for (int i = 0; i < wallRects.Length; i++)
            {
                Rect wall = wallRects[i];
                CreateWall("Inner Wall " + (i + 1), wall.center, wall.size);
            }
        }
    }

    Transform CreateBox(string objectName, Color color, Vector2 position)
    {
        GameObject box = GameObject.CreatePrimitive(PrimitiveType.Cube);
        box.name = objectName;
        box.transform.position = position;
        box.transform.localScale = Vector3.one * 0.65f;
        box.GetComponent<Renderer>().material.color = color;
        Destroy(box.GetComponent<BoxCollider>());
        return box.transform;
    }

    void CreateWall(string objectName, Vector2 position, Vector2 scale)
    {
        if (GameObject.Find(objectName) != null)
        {
            return;
        }

        GameObject wall = GameObject.CreatePrimitive(PrimitiveType.Cube);
        wall.name = objectName;
        wall.transform.position = position;
        wall.transform.localScale = new Vector3(scale.x, scale.y, 0.25f);
        wall.GetComponent<Renderer>().material.color = new Color(0.15f, 0.15f, 0.15f);
        Destroy(wall.GetComponent<BoxCollider>());
    }

    void OnGUI()
    {
        HandleFastForwardKey(Event.current);

        GUI.Label(new Rect(12, 12, 760, 28), $"Round {round} | Killer score {killerScore} | Runner goal: stop killer scoring | Time {roundTimer:F1}s | Best {bestSurvivalTime:F1}s | Speed x{speedOptions[speedIndex]:0}");
        GUI.Label(new Rect(12, 36, 900, 28), $"Q-learning: explore {exploration:F2} | Killer action {killerLastAction} | Runner action {runnerLastAction}");
        GUI.Label(new Rect(12, 60, 900, 28), $"Weights: Killer chase {killerChaseWeight:F2} predict {killerPredictWeight:F2} | Runner distance {runnerDistanceWeight:F2} wall {runnerWallWeight:F2} memory {runnerMemoryWeight:F2}");
        GUI.Label(new Rect(12, 84, 520, 28), "Fast-forward: + or ] faster, - or [ slower, 0 normal.");

        if (GUI.Button(new Rect(12, 112, 82, 30), "Normal"))
        {
            SetSpeedIndex(0);
        }

        if (GUI.Button(new Rect(100, 112, 82, 30), "Faster"))
        {
            SetSpeedIndex(speedIndex + 1);
        }

        if (GUI.Button(new Rect(188, 112, 82, 30), "Slower"))
        {
            SetSpeedIndex(speedIndex - 1);
        }

        if (GUI.Button(new Rect(276, 112, 82, 30), "x1000"))
        {
            SetSpeedIndex(speedOptions.Length - 1);
        }
    }
}
