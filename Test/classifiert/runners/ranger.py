import numpy as np

from .. import register_classifier
from ..util import run_program, get_statistics_from_time_file

def run(run_path, train_file, test_file, threads):

    run_statistics = {}
    time_file = "time.txt"
    result = run_program("ranger",
            "--file", str(train_file),
            "--depvarname", "label",
            "--treetype", "1",
            "--ntree", "150",
            "--skipoob",
            "--noreplace",
            "--fraction", "1",
            "--outprefix", "ranger",
            "--write",
            "--nthreads", str(threads),
            time_file=time_file,
            cwd=run_path)
    run_statistics.update(get_statistics_from_time_file(run_path / time_file))

    result = run_program("ranger",
            "--file", str(test_file),
            "--depvarname", "label",
            "--predict", "ranger.forest",
            "--nthreads", str(threads),
            cwd=run_path)

    with open(run_path / "ranger_out.prediction", "r") as inf:
        assert inf.readline().startswith("Predictions")
        predictions = [int(line) for line in inf]
    false_positives = np.sum(predictions[:len(predictions)//2])
    false_negatives = len(predictions) // 2 - np.sum(predictions[len(predictions)//2:])
    accuracy = 1.0 - (false_positives + false_negatives) / len(predictions)
    run_statistics["accuracy"] = accuracy

    return run_statistics

register_classifier("ranger", "csv", run)