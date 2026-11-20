# Offline student register (C)

**Wankumbu Musunga**
2nd year, distance BSc Computer Science
National Institute of Public Administration (NIPA), Lusaka, Zambia

A terminal program that keeps a class register and term results on one computer. It does not need the internet. I wrote it after a records and data-entry internship at a day secondary school in Lusaka, where learner details and marks still lived in books and in spreadsheets on the office PC.

The school in the sample, Riverside Day Secondary School, is fictional. The learners and phone numbers in `data/sample_learners.csv` are fictional too.

## The problem it solves

At the school where I did my internship, a teacher who wanted one learner's marks had to page through a mark book. Class positions were worked out with a calculator, and a small adding mistake changed who was first. The office computer could open Excel, but the internet was slow or simply not there, so an online system would have sat unused.

This program stays on that computer:

- The clerk types learners and marks into a menu.
- The program works out totals, averages and positions.
- Save writes a CSV file. Excel can open the same file, and the program can read it back after a restart.
- Nothing is uploaded anywhere.

## Features

- Add, edit, delete and search learners.
- Each learner has a name, learner (exam) number, grade (Grade 8 to 12), class, gender, and guardian phone.
- Record a mark from 0 to 100 for each subject, for Term 1, Term 2 and Term 3.
- A blank mark means the subject was not entered. It is not stored as zero, so it does not pull the average down.
- Totals, averages and class positions. If two learners have the same average they share a position and the next one is skipped (1, 1, 3).
- A class report on screen, with the option to save a text copy for printing.
- Save and load a CSV file. The register holds up to 250 learners in memory, which covers a small day school or one grade group.
- No extra libraries. `gcc` and `make` are enough.

The eight subjects are a small set used across many day-secondary classes: English Language, Mathematics, Integrated Science, Social Studies, Civic Education, Religious Education, Computer Studies and Business Studies. The real ECZ subject menu is longer, and it is not the same for Grade 8–9 and Grade 10–12.

## Grade bands (an approximation)

The Examinations Council of Zambia (ECZ) reports secondary grades on a scale from 1 to 9. ECZ standardises each subject each year. A raw percentage is not a fixed national grade, and this program must not be used to produce a certificate or an official result.

For internal term ranking, many schools still need a band next to a mark. This program uses the cuts below and says so on every class report.

| Mark (%) | Grade | Descriptor |
| --- | --- | --- |
| 75–100 | 1 | Distinction |
| 70–74 | 2 | Distinction |
| 65–69 | 3 | Merit |
| 60–64 | 4 | Merit |
| 55–59 | 5 | Credit |
| 50–54 | 6 | Credit |
| 45–49 | 7 | Satisfactory |
| 40–44 | 8 | Satisfactory |
| 0–39 | 9 | Unsatisfactory |

The overall band for a learner uses the average rounded to the nearest whole mark. A displayed average keeps one decimal place. In the sample, Chanda Mwale and Mwansa Phiri both average 69.4, so they share position 3 and the next learner is position 5.

## Sample terminal output

This is a real run against `data/sample_learners.csv` on 20 November 2026. The menu prints again after each action; those repeats are left out below. The characters after each prompt are what I typed.

```text
Loaded 11 learners from data/sample_learners.csv.

============================================================
  Riverside Day Secondary School, Lusaka
  Student register and term results (offline)
============================================================
  Learners in memory: 11
  File: data/sample_learners.csv

   1   Add a learner
   2   Edit a learner
   3   Delete a learner
   4   Search learners
   5   List all learners
   6   Record subject marks
   7   View one learner's results
   8   Class report and positions
   9   Save to CSV
  10   Load from CSV
   0   Exit
------------------------------------------------------------
Choice: 4
Name or learner number: Zulu
1 match.
No.  Learner No     Name                     Grade      Class    Gender   Guardian phone
4    2410-004       Thandiwe Zulu            Grade 10   A        Female   0954000004
```

Option 7 prints every subject. The start of Thandiwe Zulu's result looks like this:

```text
Thandiwe Zulu
Learner number: 2410-004
Grade 10 class A    Gender: Female    Guardian: 0954000004

Term 1
  English Language         81   1 Distinction
  Mathematics              77   1 Distinction
  Integrated Science       84   1 Distinction
  Social Studies           79   1 Distinction
  Civic Education          86   1 Distinction
  Religious Education      80   1 Distinction
  Computer Studies         90   1 Distinction
  Business Studies         76   1 Distinction
  Total: 653    Average: 81.6    Approx. grade: 1 Distinction
```

Term 2 is printed in the same shape (total 673, average 84.1). Term 3 says that no marks were entered.

The class report for Grade 10A, Term 1:

```text
Choice: 8
Grade (8-12): 10
Class: A
Term (1-3): 1

Riverside Day Secondary School, Lusaka
Class report
Grade 10 class A | Term 1
Printed: 20 November 2026
------------------------------------------------------------
Pos  Learner No   Name                    Eng  Mat  Sci  Soc  Civ  R.E  Com  Bus Total    Avg  Band
1    2410-004     Thandiwe Zulu            81   77   84   79   86   80   90   76   653   81.6  1 Distinction
2    2410-001     Luyando Banda            78   82   74   69   80   71   88   64   606   75.8  1 Distinction
3    2410-006     Chanda Mwale             70   70   70   70   70   70   70   65   555   69.4  3 Merit
3    2410-002     Mwansa Phiri             70   66   72   68   74   69   71   65   555   69.4  3 Merit
5    2410-003     Kondwani Tembo           61   58   63   55   60   64   59   62   482   60.3  4 Merit
6    2410-005     Joseph Mulenga           48   52   44   50   47   55   41   49   386   48.3  7 Satisfactory
------------------------------------------------------------
Learners in this class: 6
Ranked (at least one mark): 6
Class average: 67.5

Subjects:
  Eng  English Language
  Mat  Mathematics
  Sci  Integrated Science
  Soc  Social Studies
  Civ  Civic Education
  R.E  Religious Education
  Com  Computer Studies
  Bus  Business Studies
Total adds up the marks that were entered. Average divides that
total by how many subjects have a mark. A blank is not a zero.
Positions: the highest average is 1. A tie shares a position
and the next one is skipped (for example 1, 1, 3).
Grade bands approximate the ECZ 1-9 scale for school use.
They are not an official Examinations Council of Zambia result.
Save a text copy? (y/n): n
```

Choosing `y` asks for a file name and writes the same report as plain text. The screen scrolls, so an earlier report stays above the next menu.

## Build and run

You need `gcc` and `make`. There is nothing to install.

```bash
make
make test
cp data/sample_learners.csv data/learners.csv
./build/schoolreg
```

`make run` does the copy and starts the program for you. The working file is `data/learners.csv`. It is not committed, so saving from the menu does not change the sample.

You can also pass a path:

```bash
./build/schoolreg data/learners.csv
```

`make test` rebuilds with `-Wall -Wextra -Wpedantic -Werror`, runs the checks in `tests/test_register.c`, then drives the menu from `tests/e2e.sh`.

While the program is open:

- Option 9 saves the CSV.
- Option 10 loads a CSV. If you have unsaved edits, it asks before replacing them.
- Option 0 asks whether to save if something has changed.
- Closing the terminal with Ctrl+C skips that question, so save first.

The CSV has one row per learner. Empty mark cells are missing marks. A name that contains a comma is quoted, which is what keeps it in one Excel cell. The header row has to stay as the program wrote it.

To point the printout at another school, change `SCHOOL_NAME` and `SCHOOL_TOWN` at the top of `src/main.c`.

## Project structure

```text
.
├── LICENSE
├── Makefile
├── README.md
├── data
│   └── sample_learners.csv    fictional Grade 8, 10 and 12 learners
├── src
│   ├── main.c                  menu, prompts and the class report
│   ├── student.h               the register, bands and positions
│   ├── student.c
│   ├── storage.h               CSV save and load
│   └── storage.c
└── tests
    ├── test_register.c         bands, ties, validation, CSV round trip
    └── e2e.sh                  scripted menu session
```

`student.c` holds the school rules. `storage.c` holds the file format. `main.c` only talks to the person at the keyboard. The sample file is the same CSV the program saves, including a blank Social Studies mark for Bupe Ngoma so a missing mark is visible:

```text
2410-009,Bupe Ngoma,10,B,F,0979000009,55,60,58,,62,57,64,50,,,,,,,,,,,,,,,,
```

The empty field between 58 and 62 is the missing mark. The commas after 50 are Terms 2 and 3, which have not been entered.

## What I learned

On the internship I mostly typed. The useful part was seeing what went wrong on paper: two learners with the same surname, a total that had been added one mark short, and a phone number written with spaces. I wanted the program to be careful about those three things.

Splitting the code into three files made that easier. The grade bands and the position rule live in `student.c`, so the menu cannot accidentally rank a class a second, different way. I compare two averages by cross-multiplying (`total_a * count_b` against `total_b * count_a`) so a tie is exact and I do not have to compare decimal numbers.

Names are read with `fgets`. A name such as "Thandiwe Zulu" has a space, and reading it with `scanf` would leave the surname waiting in the input for the next question. `fgets` takes the whole line. Prompts are flushed before each read, because a piped test would otherwise wait on a prompt that was still sitting in the output buffer.

A mark of 0 and a blank mark are different. I store a blank as `-1` (`MARK_NONE`). Saving that as an empty CSV cell, rather than as 0, means Excel shows a gap and the average stays fair. Saving writes `file.csv.tmp` and then renames it, so a save that stops halfway does not destroy the previous register.

The fixed array of 250 learners is a limit I have to say out loud. It is enough for this project and it keeps the program free of `malloc`. A bigger school would need a different structure.

## Future ideas

- Separate subject lists for Grade 8–9 and Grade 10–12, closer to the real ECZ menu.
- A one-page report card for a single learner, as well as the class list.
- Attendance, which was the other book I had to update on the internship.
- A dated backup next to the CSV every time it is saved.
- Ask for the school name the first time the program runs, instead of editing `main.c`.
- A simple password so a learner cannot open the file and change a mark.
- A windowed version later, after the rules on this command-line version are solid.

## License

MIT. See [LICENSE](LICENSE).
