"""The letters of the sign, in its own typeface (measured from VINEWOOD): 15.74 m tall, strokes 3.38 m, counters 2.24 m,
45-degree chamfers of 2.25 m, bars 3.38 m with 2.80 m between them. Outlines in metres, x right and z up from the letter's
bottom left; the outer loop counter-clockwise, holes clockwise."""

H = 15.74
ST = 3.38          # stroke
IN = 5.62          # 9 - stroke: the right stroke's inner edge in a 9 m letter
C = 2.25           # chamfer
MID0, MID1 = 6.18, 9.56     # the middle bar (as E's)
TOP0, BOT1 = 12.36, 3.38    # the top bar's bottom, the bottom bar's top

WIDTH = {"S": 9.0, "T": 9.0, "A": 10.341, "L": 7.868, "G": 9.0, "N": 9.0, "O": 9.0, "I": ST}

# A: V upside down (its legs 3.357 m wide at the foot, 4.496 m together at the top), with a crossbar
_k = (5.161 - 3.357) / 10.482      # the inner edges' slope (x per z)
_XB0, _XB1 = 2.4, 5.0              # the crossbar


def _inner_left(z):
    return 3.357 + _k * z


def _inner_right(z):
    return 6.964 - _k * z


OUTLINES = {
    "S": [[
        (9.0, 11.24), (9.0, H - C), (9.0 - C, H), (C, H), (0.0, H - C), (0.0, MID0 + C), (C, MID0), (IN, MID0),
        (IN, BOT1), (ST, BOT1), (ST, 4.50), (0.0, 4.50), (0.0, C), (C, 0.0), (9.0 - C, 0.0), (9.0, C),
        (9.0, MID1 - C), (9.0 - C, MID1), (ST, MID1), (ST, TOP0), (IN, TOP0), (IN, 11.24),
    ]],
    "T": [[
        (2.81, 0.0), (6.19, 0.0), (6.19, TOP0), (9.0, TOP0), (9.0, H), (0.0, H), (0.0, TOP0), (2.81, TOP0),
    ]],
    "A": [
        [(0.0, 0.0), (3.357, 0.0), (_inner_left(_XB0), _XB0), (_inner_right(_XB0), _XB0), (6.964, 0.0),
         (10.341, 0.0), (7.418, H), (2.922, H)],
        [(_inner_left(_XB1), _XB1), (5.1605, 10.482), (_inner_right(_XB1), _XB1)],
    ],
    "L": [[
        (0.0, 0.0), (7.868, 0.0), (7.868, BOT1), (ST, BOT1), (ST, H), (0.0, H),
    ]],
    "G": [[
        (C, 0.0), (9.0 - C, 0.0), (9.0, C), (9.0, 8.98), (4.5, 8.98), (4.5, MID0), (IN, MID0), (IN, BOT1),
        (ST, BOT1), (ST, TOP0), (IN, TOP0), (IN, 11.24), (9.0, 11.24), (9.0, H - C), (9.0 - C, H), (C, H),
        (0.0, H - C), (0.0, C),
    ]],
    # as VINEWOOD's own (for the far-away model and the collision: the game's fronts are copied as they are)
    "N": [[
        (0.0, 0.0), (ST, 0.0), (ST, 6.284), (IN, 0.0), (9.0, 0.0), (9.0, H), (IN, H), (IN, 9.432), (3.372, H),
        (0.0, H),
    ]],
    "O": [
        [(0.0, C), (C, 0.0), (9.0 - C, 0.0), (9.0, C), (9.0, H - C), (9.0 - C, H), (C, H), (0.0, H - C)],
        [(ST, BOT1), (ST, TOP0), (IN, TOP0), (IN, BOT1)],
    ],
    "I": [[(0.0, 0.0), (ST, 0.0), (ST, H), (0.0, H)]],
}
