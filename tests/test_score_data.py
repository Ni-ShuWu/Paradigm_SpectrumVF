import unittest

from score_data import ChartNote, ScoreChart, edge_position


class ScoreChartTests(unittest.TestCase):
    def test_round_trip_keeps_edge_and_space_note_fields(self):
        chart = ScoreChart(
            "起源测试",
            [
                ChartNote("EdgeNote", "tap", 96, False, edge=0, pos=4.5),
                ChartNote("SpaceNote", "flick", 144, True, x=6.0, y=3.5),
            ],
        )
        loaded = ScoreChart.from_json(chart.to_json())
        self.assertEqual(loaded.title, chart.title)
        self.assertEqual([n.to_dict() for n in loaded.notes], [n.to_dict() for n in chart.notes])

    def test_parses_game_geometry_for_all_edges(self):
        for edge, expected in ((0, (0, 3)), (1, (12, 3)), (2, (3, 9)), (3, (3, 0))):
            note = ScoreChart.from_json(
                '{"notes":[{"type":"EdgeNote","edge":%d,"pos":3,"tick":1}]}' % edge
            ).notes[0]
            self.assertEqual(note.coordinates(), expected)

    def test_projects_tap_position_onto_selected_edge(self):
        self.assertEqual(edge_position(0, 8, 2), 2)
        self.assertEqual(edge_position(2, 8, 2), 8)

    def test_rejects_out_of_range_positions(self):
        with self.assertRaisesRegex(ValueError, "x 必须在"):
            ScoreChart.from_json('{"notes":[{"type":"SpaceNote","x":13,"y":2}]}')

    def test_rejects_invalid_edge_and_non_integer_tick(self):
        for note in (
            '{"type":"EdgeNote","edge":4,"pos":2}',
            '{"type":"SpaceNote","x":2,"y":2,"tick":1.5}',
        ):
            with self.subTest(note=note), self.assertRaises(ValueError):
                ScoreChart.from_json('{"notes":[' + note + ']}')

    def test_rejects_invalid_fake_flag_and_non_finite_coordinates(self):
        for note in (
            '{"type":"SpaceNote","x":2,"y":2,"isFake":0}',
            '{"type":"SpaceNote","x":NaN,"y":2}',
        ):
            with self.subTest(note=note), self.assertRaises(ValueError):
                ScoreChart.from_json('{"notes":[' + note + ']}')

    def test_rejects_unsupported_format_version_and_note_type(self):
        for document in (
            '{"format":"other","notes":[]}',
            '{"version":2,"notes":[]}',
            '{"notes":[{"type":"Unknown"}]}',
        ):
            with self.subTest(document=document), self.assertRaises(ValueError):
                ScoreChart.from_json(document)


if __name__ == "__main__":
    unittest.main()
