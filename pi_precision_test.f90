program pi_precision_test
 ! Program to calculate pi and output with increasing precision
 ! Purpose: Test uband_diff's ability to recognize that values
 ! at different precisions (3.1 vs 3.14) are equivalent when
 ! their raw difference is within the sub-LSB tolerance

implicit none

 ! Variables for pi calculation
real(kind=8) :: pi_calculated
real(kind=8) :: epsilon_machine
integer :: max_decimal_places

 ! Loop variables
integer :: i
character(len=100) :: output_filename
character(len=20) :: format_string
integer :: unit_num

 ! Calculate pi using arctan series: pi = 4 * arctan(1)
 ! Using Machin's formula: pi/4 = 4*arctan(1/5) - arctan(1/239)
pi_calculated = calculate_pi()

 ! Determine machine epsilon for double precision
epsilon_machine = epsilon(pi_calculated)

 ! Maximum decimal places we can trust is based on machine precision
 ! For double precision (64-bit), epsilon ~ 2.22e-16
 ! This gives us about 15-16 significant decimal digits
 ! Since pi ~ 3.14159..., we can trust about 14-15 decimal places
max_decimal_places = -int(log10(epsilon_machine)) - 1

 ! Cap at 15 for double precision (conservative)
if (max_decimal_places > 15) max_decimal_places = 15

 ! Get output filename from command line or use default
if (command_argument_count() > 0) then
   call get_command_argument(1, output_filename)
else
   output_filename = 'pi_output.txt'
end if

 ! Open output file
unit_num = 10
open(unit=unit_num, file=trim(output_filename), &
   status='replace', action='write')

 ! Write header comment
write(unit_num, '(a)') &
   '! Pi calculated with increasing decimal precision'
write(unit_num, '(a,es12.5)') '! Machine epsilon: ', &
   epsilon_machine
write(unit_num, '(a,i0)') &
   '! Maximum valid decimal places: ', max_decimal_places
write(unit_num, '(a)') '! Format: index value'

 ! Loop over decimal places from 0 to max
do i = 0, max_decimal_places
   ! For 0 decimal places, use integer format
   if (i == 0) then
      write(unit_num, '(i0,1x,i0)') i+1, int(pi_calculated)
   else
      ! Build format string dynamically: f<width>.<decimals>
      ! Width needs to accommodate: sign + integer part +
      ! decimal point + decimal places
      write(format_string, '(a,i0,a,i0,a)') &
         '(i0,1x,f', i+3, '.', i, ')'
      write(unit_num, format_string) i+1, pi_calculated
   end if
end do

close(unit_num)

 ! Print summary to console
print '(a)', 'Pi Precision Test Program'
print '(a)', '=========================='
print '(a,f20.15)', 'Calculated pi:           ', pi_calculated
print '(a,es12.5)', 'Machine epsilon:         ', epsilon_machine
print '(a,i0)', 'Max valid decimal places:', max_decimal_places
print '(a,a)', 'Output written to:       ', trim(output_filename)
print '(a,i0,a)', 'Generated ', max_decimal_places + 1, ' lines'

contains

function calculate_pi() result(pi)
 ! Calculate pi using built-in atan function
 ! Simple and straightforward: pi = 4 * atan(1)

implicit none
real(kind=8) :: pi

 ! Use intrinsic atan function (simplest method)
pi = 4.0d0 * atan(1.0d0)

end function calculate_pi

end program pi_precision_test
