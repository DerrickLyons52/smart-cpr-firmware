# CPR scoring thresholds

MIN_DEPTH_MM = 50.8
MAX_DEPTH_MM = 61.0
MAX_LEAN_MM = 20.0

MIN_RATE_CPM = 100.0
MAX_RATE_CPM = 120.0

MAX_HAND_OFFSET_MM = 12.0


def average(values):
    values = [v for v in values if v is not None]

    if not values:
        return 0.0

    return sum(values) / len(values)


def hand_offset(row):
    hp_x = row.get("hp_x", 0.0)
    hp_y = row.get("hp_y", 0.0)
    return (hp_x ** 2 + hp_y ** 2) ** 0.5


def score_session(rows):
    """
    Takes per-compression rows and scores the session using averages.
    """

    if not rows:
        return {
            "avg_depth": 0.0,
            "avg_rate": 0.0,
            "avg_lean": 0.0,
            "avg_force": 0.0,
            "avg_hp_offset": 0.0,
            "passed": False,
            "total_compressions": 0,
        }

    depths = [row.get("peak_depth_mm", 0.0) for row in rows]
    rates = [row.get("rate_cpm", 0.0) for row in rows]
    leans = [row.get("lean_mm", 0.0) for row in rows]
    forces = [row.get("force_N", 0.0) for row in rows]
    hand_offsets = [hand_offset(row) for row in rows]

    avg_depth = average(depths)
    avg_rate = average(rates)
    avg_lean = average(leans)
    avg_force = average(forces)
    avg_hp_offset = average(hand_offsets)

    passed = (
            MIN_DEPTH_MM <= avg_depth <= MAX_DEPTH_MM and
            MIN_RATE_CPM <= avg_rate <= MAX_RATE_CPM and
            avg_lean <= MAX_LEAN_MM and
            avg_hp_offset <= MAX_HAND_OFFSET_MM
    )

    return {
        "avg_depth": avg_depth,
        "avg_rate": avg_rate,
        "avg_lean": avg_lean,
        "avg_force": avg_force,
        "avg_hp_offset": avg_hp_offset,
        "passed": passed,
        "total_compressions": len(rows),
    }