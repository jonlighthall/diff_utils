bbpp.freq
   scientific format
   fixed data width (varying decimal places)
   why is the "range" different between 1 and 3?

bbpp.rc
   STATUS: breaks
   FIX: crazy output formatting

bbpp.ts
   test for tsdiff functionality
   FIX: can't handle number of decimal places

bbtl.nspe02
   FIX: bad message
   FIX: no percentage, but probably is printed

calcpduct.std0
   test cpddiff
   has header line

edge_case
   FIX fail comparison when set to zero

file (case 2)
   odd case
   starts out the same

nspe.std2.
   another great example
   this should "fail" but should pass with the 2% rule

nspe.std3
   considering the ouptut of ./bin/uband-diff data/pe.std3.test.nspe01.txt data/pe.std3.refe.nspe01.txt

   this is a realistic example of data being compared.

   something got changed in the way differences are processed. in this example, the two files should "fail" a simple diff, but if we are also taking into account some kind of percentage difference, that needs to be stated explicitly and printed. if the arguments say that 0 or 0.05 is a failure and the maximum diff exceeds that, it should fail.

   i had considered as a rule of thumb if only x-percent, say 2%, of entries had differences that exceeded the significant threshold, then the case should pass. that got removed in a recent commit. However, this concept gets back to an idea I had to more precisely determine if the "significant" differences between two files were really significant. Namely, count how many lines does the error span. I would like to bring back one or both of those concepts. If the differences are less than 2% of the total number of elements compared, then it should pass. or if no run of errors exceeds X number of lines, it should pass. In any event, if something other than a straight difference is being used to determine if the file passes or fails, that needs to be made explicit. because right now, naively, these files should fail.

   I think it's showing no significant differences because it may be failing at the point where different structures are identified. in this case the "ranges" appear to be different because in file nspe.std3.refe.nspe01.asc, the precision of the printed range is too small to capture the step size. in both files the range step size is 0.0122, however in the reference file, the printed precision of the first column is too small to capture the step size at every step, so it fails the monotonically increasing test, which is actually incorrect because monotonically increasing should allow for the range to be unchanged.


   make && ./bin/uband-diff data/nspe.std3.test.nspe01.txt data/nspe.std3.refe.nspe01.txt 0.2 3 .6

   I need to fix the output so that if we're just going by an absolute difference, this file should fail. it still stays equivalent within tolerance and it still says the files pass the significant difference test.

   I tried to add an override to the significant difference failure test. The condition of the override was that if the number of significant differences, in this case 905, was less than 2 percent of the total (in this case 3554*.02 = 71), then it should pass. so in this case it should definitely fail because it has significant differences and the number of significant differences is 905/3554=25% of the total, which is more than the override quantity 2%

   let's add a count of printed differences in the summary (for cases where the table probably overflows the terminal)

    if we want to allow percentage thresholds, I think that should be introduced through the arguments and be stated explicitly. For example, when the threshold is "0.05", then any difference exceeding 0.05 should be counted as a failure. My idea is that if the same argument, the third argument, is made negative, say "-10", then the threshold would change to a relative percentage threshold of 10%, so a realistic value for a relative percentage threshold would be something like 1 or 0.1. Is there a way to estimate what the minimum difference between two numbers could be at a certain scale in single precision arithmetic? I assume the minimum difference would be... different if the values being compared are on the order of 10^-5 (for time series or pressure data) or 10^+2 (for typical transmission loss data) or 10^5 for column 1 range data.

    I don't think trivial differences should get printed in the table, unless the debug/print level is non-zero. can you help me update that

case 1

   $ ./bin/uband-diff data/case1r.tl data/case1r_01.txt 0 1 0.01 1

    let's look at this example. with a threshold of zero, there are 39 differences (correctly tallied). there are 12 differences where the corresponding values are above the ignore threshold of 138.47; these are trivial and should be ignored. there are 24 differences where the corresponding values exceed the marginal threshold. these should be counted, but not cause a error or failure in the difference (by themselves). there are 3 differences where the values are less than the marginal threshold and should therefore be counted as significant differences. however, in this case, the file should pass because the percentage of number of errors is small. I think there are supposed to be 39-12=27 significant differences, not 13, so I don't know where that number is coming from. It looks like the non-zero differences are being double counted (39x2=78). also, if I set the print threshold to zero, all of the zero differences are printed. we should update this so that anything greater than the printed threshold gets printed, not equal to. we need to check the tallies and make sure everything is self consistent.

    I want to check the logic related to the difference tallies which are printed at the end of the program in the summary.

    Let's look at the following command for example
   $ ./bin/uband-diff data/case1r.tl data/case1r_01.txt 0 1 0 1

    the number of elements is obvious, that should be the total number of elements compared. in this case the file has 353 lines with 11 columns per line (1 range with 10 data columns), so the total number of elements is 3883

the number of elements breaks down into zero-difference elements and non-zero difference elements. Again, nothing fancy here. If the raw difference is non-zero, that counts as a non-zero difference. every element has either a zero or non-zero difference, so the two tallies should sum to match the number of elements. In this case, there are 39 non-zero differences (which means there should be 3844 zero differences)

we are not concerned with the elements that are identical. the non-zero differences breakdown further into trivial and non-trivial differences. this is the cornerstone of the program. determining when a difference in format or precision between elements in two files corresponds to a legitimate difference or not. this is where the minimum number of decimal places between the elements comes into the calculation. all of the non-zero differences are either trivial or non-trivial. they should add up to the number of non-zero differences. In this case all of the 39 non-zero differences are non-trivial.

the non-trivial differences are the differences that matter. here is where we apply the user-input significant threshold to determine if the non-trivial difference warrants deeming the files dissimilar.

non-trivial differences break down into significant and insignificant differences. the significance of the difference is based on the magnitude of the difference AND the magnitude of the individual values. if both of the values is greater than the ignore value threshold of ~138, the difference is insignificant. These are the blue entries in the table. all other differences are significant. insignificant differences are related to the limitations of machine precision with single precision numbers. the ignore value threshold is related to the smallest pressure that can be represented in single precision and then converting that to transmission loss. By my count, there are 12 "insignificant" differences where the magnitude of both vales exceeds the ignore threshold of ~138 (corresponding to pressure values less than the single precision epsilon of ~1.19E-7), which means there should be 27 significant differences (those differences that can't be written off as being due to machine precision and correspond to differences greater than the user threshold (first argument after file names)). HOWEVER, ONLY 10 DIFFERENCES ARE REPORTED AS SIGNIFICANT. WE NEED TO UPDATE THIS LOGIC.

significant differences are divided into marginal and non-marginal (is there a better word for non-marginal). Again, this division is based on the magnitude of the individual values AND the magnitude of the difference. marginal differences occur when both values are greater than the marginal value threshold of 110 and less than the ignore value threshold of ~138. these differences are not practically important, but they do likely represent a computational error. In this example, of the 27 significant differences, I count 24 differences that have both values in the marginal range. these are the values printed in yellow in the table. however, the program is reporting 9 marginal differences. there are 3 non-marginal cases (the last three in the table). The marginal/non-marginal count should be 24 and 3, which adds up to the significant difference tally of 27.

the non-marginal differences are divided into critical and non-critical differences. this is based on the value of the critical difference threshold. differences exceeding the critical difference threshold represent a potential model failure and corresponds to a failed comparison. Like the non-critical differences, the critical differences only count if they are significant based on the magnitude of the values. This is our first failure criteria: if the difference of a significant differences exceeds the critical difference threshold threshold. In this case there are no critical differences.

the remaining differences are the ones of real interest. the non-critical, non-marginal, significant differences. these cannot be attributed to model failure, or ignored due to the values being outside of operational interest, or by corresponding to possible errors in machine precision. in this case there are 3 non-marginal significant errors.

if the number of non-marginal, non-critical, significant differences is greater than 2% of the total number of elements, the file comparison should fail. if the number of differences is less than 2%, the file comparison passes, but a note should be issued to the user that the files are "close enough" but not identical. In this case 3 out of the 3883 differences correspond to 0.0773%.

I would like to update the counting of differences to reflect these numbers (unless I've made an error in my calculations)

I think you found the answer. The logic of significant/insignificant should use the user threshold and not the  threshold derived from the decimal place precision.

The following summations should be true at each analysis level. once we figure this out, we should add each of these to the unit testing.

LEVEL 1: total = zero + non_zero (based on raw difference [calculated])
LEVEL 2: non_zero = insignificant + significant (based on ignore value threshold of ~138 [hard coded, based on machine precision])
LEVEL 3: significant = marginal + non_marginal (based on marginal value threshold of 110 [hard coded, based on research])
LEVEL 4: non_marginal = critical + non_critical (based on critical difference threshold [argument 4])

therefore...

at LEVEL 2:
total = zero + insignificant + significant

at LEVEL 3:
total = zero + insignificant + marginal + non_marginal
non_zero = insignificant + marginal + non_marginal

at LEVEL 4:
total = zero + insignificant + marginal + critical + non_critical
non_zero = insignificant + marginal + critical + non_critical
significant = marginal + critical + non_critical


I left out the final analysis/discrimination level which takes into account the user threshold:

LEVEL 1: total = zero + non_zero (based on raw difference [calculated])
LEVEL 2: non_zero = insignificant + significant (based on ignore value threshold of ~138 [hard coded, based on machine precision])
LEVEL 3: significant = marginal + non_marginal (based on marginal value threshold of 110 [hard coded, based on research])
LEVEL 4: non_marginal = critical + non_critical (based on critical difference threshold [argument 4])
LEVEL 5: non_critical = error + non_error (based on user threshold [argument 3])

therefore...

at LEVEL 2:
total = zero + insignificant + significant

at LEVEL 3:
total = zero + insignificant + marginal + non_marginal
non_zero = insignificant + marginal + non_marginal

at LEVEL 4:
total = zero + insignificant + marginal + critical + non_critical
non_zero = insignificant + marginal + critical + non_critical
significant = marginal + critical + non_critical

at LEVEL 5:
total = zero + insignificant + marginal + critical + error + non_error
non_zero = insignificant + marginal + critical + + error + non_error
significant = marginal + critical + + error + non_error

are any of these missing from the unit testing? let's check the unit testing, it looks like one of the tests is failing.

I think you caught an error in my logic. At each level, (a subset of) the data is divided into two groups. One of the two divisions is further divided in the next step. Let's update the logic so that each division only contains two groups as outlined below.

I also forgot another discrimination level that comes in between level 1 and level 2: the trivial/non-trivial differences

LEVEL 0: total = sum over column groups: (lines per column group)*(columns in column group)
         -> if number of elements do not match, then the file comparison is a failure, but still process element differences up to the difference in file structure
         e.g. 353 lines * 11 columns = 3883 elements
LEVEL 1: total = zero + non_zero (based on raw difference [calculated])
         -> if there are no non-zero differences, the files are the same; this file comparison should give the same results as "diff" with the added functionality that any trailing zeros due to differences in format are essentially ignored (e.g. 1.5 vs 1.5000 would fail a "diff" test, but would pass the raw difference test assuming the zero threshold is set appropriately)
         e.g. 3883 = 3844 + 39 = 353 * 11
I realized we're missing a discrimination level:
LEVEL 2: [new] non_zero = trivial + non_trivial (based on printed precision)
         -> if the printed numbers are the same to within the minimum printed precision, then the values match. here is the main functional addition over "diff": differences are actually calculated, taking the minimum printed precision into account. it is the first place where the assumptions of the algorithm play into how the difference between the files is calculated
         e.g. 39 = 0 + 39 (no trivial differences)
LEVEL 3: [old 2] non_trivial = insignificant + significant (based on ignore value threshold of ~138 [hard coded, based on machine precision])
         -> this introduces the domain-specific requirements of the assumed data set. if we are comparing transmission loss, then those values correspond to pressures. if we calculate the TL for a pressure corresponding to the single precision machine epsilon, we get ~138. TL values greater than this are meaningless or unreliable depending on how the individual computer handles small number arithmetic.
         e.g. 39 = 12 + 27
LEVEL 4: significant = marginal + non_marginal (based on marginal value threshold of 110 [hard coded, based on research])
         -> this is a further domain specific division. When TL differences are calculated in a research environment, TL values of 110 and greater are weighted zero. just as non-zero and non-trivial differences have their own section in the summary, we should add additional sections to the summary (delimited with dashed lines) which shows 1) the split between the refined definition of significant/insignificant and 2) the marginal/non-marginal differences
         e.g. 27 = 24 3
LEVEL 5: non_marginal = critical + non_critical (based on critical difference threshold [argument 4])
         -> as teh program is written now, this is really just a sanity check because a non-marginal critical difference will stop the program.
         e.g. 3 = 0 + 3 (the critical count will only ever be one or zero. Let's update the logic so that the table prints are aborted if a critical error is hit so that the rest of the file will be processed)
LEVEL 6: non_critical = error + non_error (based on user threshold [argument 3])
         e.g. 3 = 3 + 0

therefore...

at LEVEL 1:
sum over column groups: (lines per column group)*(columns in column group) = zero + non_zero

at LEVEL 2:
total = zero + trivial + non_trivial

at LEVEL 3:
non_zero = trivial + insignificant + significant
total = zero + trivial + insignificant + significant

at LEVEL 4:
non_trivial = insignificant + marginal + non_marginal
non_zero = trivial + insignificant + marginal + non_marginal
total = zero + trivial + insignificant + marginal + non_marginal

at LEVEL 5:
significant = marginal + critical + non_critical
non_trivial = insignificant + marginal + critical + non_critical
non_zero = trivial + insignificant + marginal + critical + non_critical
total = zero + trivial + insignificant + marginal + critical + non_critical

at LEVEL 6:
non_marginal = critical + error + non_error
significant = marginal + critical + error + non_error
non_trivial = insignificant + marginal + critical + error + non_error
non_zero = trivial + insignificant + marginal + critical + error + non_error
total = zero + trivial + insignificant + marginal + critical + error + non_error

are any of these missing from the unit testing? let's check the unit testing, it looks like one of the tests is failing.

could it be because the critical error exits the analysis? we should change that


Let's use the following command as an example
./bin/uband_diff data/pe.std1.pe01.ref.txt data/pe.std1.pe01.test.txt 0 1 0 1

Since writing this program, I have revised the logic that I would like the algorithm to use. I need help checking the logic and update the algorithm to use the new hierarchy of binary divisions, outlined below.

The program goes through six levels of discrimination to determine the level of agreement between two files.

First (level 0), the number of elements is determined:

LEVEL 0: total = sum over column groups: (lines per column group)*(columns in column group)
         -> if number of elements do not match, then the file comparison is a failure, but still process element differences up to the difference in file structure
         e.g. 353 lines * 11 columns = 3883 elements

Then the data set is divided into two groups based on the raw (unrounded, unadjusted) difference between the numerical elements of each file

LEVEL 1: total = zero + non_zero (based on raw difference [calculated])
         -> if there are no non-zero differences, the files are the same; this file comparison should give the same results as "diff" with the added functionality that any trailing zeros due to differences in format are essentially ignored (e.g. 1.5 vs 1.5000 would fail a "diff" test, but would pass the raw difference test assuming the zero threshold is set appropriately)
         I modified the first line of pe.std1.pe01.ref.txt to add extra decimal places to the values which would all round to the original value
         I changed all ten "TL" elements. one of them I added a trailing zero, the other nine I added an extra decimal place with a value between 1-4
         there are 39 real differences in the file plus the 9 trivial and 1 non-trival differences I introduced
         therefore, there are 48 non-zero differences int his file
         e.g. 3883 = 3835 + 48

LEVEL 2: non_zero = trivial + non_trivial (based on the format precision in the files)
         -> if the printed numbers are the same to within the minimum printed precision, then the values match. here is the main functional addition over "diff": differences are actually calculated, taking the minimum printed precision into account. it is the first place where the assumptions of the algorithm play into how the difference between the files is calculated
         Here, the 9 trivial differences I added are identified
         e.g. 48 = 9 + 39 (no trivial differences)

In this example, there are 9 non-trivial differences that I introduced manually on line 1.

LEVEL 3: non_trivial = insignificant + significant (based on ignore value threshold of ~138 [hard coded, based on machine precision])
         -> this introduces the domain-specific requirements of the assumed data set. if we are comparing transmission loss, then those values correspond to pressures. if we calculate the TL for a pressure corresponding to the single precision machine epsilon, we get ~138. TL values greater than this are meaningless or unreliable depending on how the individual computer handles small number arithmetic.
         e.g. 39 = 12 + 27

In this example there are 27 significant differences

LEVEL 4: significant = marginal + non_marginal (based on marginal value threshold of 110 [hard coded, based on research])
         -> this is a further domain specific division. When TL differences are calculated in a research environment, TL values of 110 and greater are weighted zero. just as non-zero and non-trivial differences have their own section in the summary, we should add additional sections to the summary (delimited with dashed lines) which shows 1) the split between the refined definition of significant/insignificant and 2) the marginal/non-marginal differences
         e.g. 27 = 24 3

         there are 3 non-marginal differences in this example

LEVEL 5: non_marginal = critical + non_critical (based on critical difference threshold [argument 4])
         -> as the program is written now, this is really just a sanity check because a non-marginal critical difference will stop the program.
         e.g. 3 = 0 + 3 (the critical count will only ever be one or zero. Let's update the logic so that the table prints are aborted if a critical difference is found so that the rest of the file will be processed)

         finally, the data is divided into error-causing differences and non-error causing differences. This is based on the significant threshold. If the file has significant differences that are non-marginal and non-critical, the files do not match. However, if less than 2% of the elements have significant differences, this is overridden and a pass is return to the parent program and a note that the files are probably close enough

LEVEL 6: non_critical = error + non_error (based on user threshold [argument 3])
         e.g. 3 = 3 + 0

therefore...

at LEVEL 1:
sum over column groups: (lines per column group)*(columns in column group) = zero + non_zero

at LEVEL 2:
total = zero + trivial + non_trivial

at LEVEL 3:
non_zero = trivial + insignificant + significant
total = zero + trivial + insignificant + significant

at LEVEL 4:
non_trivial = insignificant + marginal + non_marginal
non_zero = trivial + insignificant + marginal + non_marginal
total = zero + trivial + insignificant + marginal + non_marginal

at LEVEL 5:
significant = marginal + critical + non_critical
non_trivial = insignificant + marginal + critical + non_critical
non_zero = trivial + insignificant + marginal + critical + non_critical
total = zero + trivial + insignificant + marginal + critical + non_critical

at LEVEL 6:
non_marginal = critical + error + non_error
significant = marginal + critical + error + non_error
non_trivial = insignificant + marginal + critical + error + non_error
non_zero = trivial + insignificant + marginal + critical + error + non_error
total = zero + trivial + insignificant + marginal + critical + error + non_error

if the data in column 1 is monotonically increasing, let's assume the column data is range. in which case, the TL threshold checks of 138 and 110 db are inapplicable.