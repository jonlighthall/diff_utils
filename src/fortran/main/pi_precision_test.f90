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
character(len=100) :: output_filename
integer :: l
character(len=200) :: asc_name, desc_name

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

 ! Get base filename from command line or use default
if (command_argument_count() > 0) then
   call get_command_argument(1, output_filename)
else
   output_filename = 'pi_output.txt'
end if

! Normalize base filename by stripping trailing .txt if present
l = len_trim(output_filename)
if (l > 4) then
   if (output_filename(l-3:l) == '.txt') then
      output_filename = output_filename(1:l-4)
   else
      output_filename = trim(output_filename)
   end if
else
   output_filename = trim(output_filename)
end if

! Program identifier (standardized)
write(*,*) 'Using program id: pi_gen_fortran'

! Build output filenames (standardized): pi_fortran_asc.txt / pi_fortran_desc.txt
! Use fixed standardized filenames so scripts can find them reliably
asc_name = 'pi_fortran_asc.txt'
desc_name = 'pi_fortran_desc.txt'

! Generate ascending precision file (0 to max decimal places)
call write_precision_file(asc_name, pi_calculated, 0, &
   max_decimal_places, 1)

! Generate descending precision file (max to 0 decimal places)
call write_precision_file(desc_name, pi_calculated, &
   max_decimal_places, 0, -1)

 ! Print summary to console
print '(a)', 'Pi Precision Test Program'
print '(a)', '=========================='
print '(a,f20.15)', 'Calculated pi:           ', pi_calculated
print '(a,es12.5)', 'Machine epsilon:         ', epsilon_machine
print '(a,i0)', 'Max valid decimal places:', max_decimal_places
print '(a)', ''
print '(a,a)', 'Ascending file:  ', &
   asc_name
print '(a,a)', 'Descending file: ', &
   desc_name
print '(a,i0,a)', 'Each contains ', max_decimal_places + 1, ' lines'

contains

function calculate_pi() result(pi)
 ! Calculate pi using built-in atan function
 ! Simple and straightforward: pi = 4 * atan(1)

implicit none
real(kind=8) :: pi

 ! Use intrinsic atan function (simplest method)
pi = 4.0d0 * atan(1.0d0)

end function calculate_pi

subroutine write_precision_file(filename, value, start_dp, &
   end_dp, step)
 ! Write value with varying decimal precision to file
 ! Arguments:
 !   filename: output file name
 !   value: the number to write (e.g., pi, sqrt(2), etc.)
 !   start_dp: starting decimal places
 !   end_dp: ending decimal places
 !   step: increment (+1 for ascending, -1 for descending)

implicit none
character(len=*), intent(in) :: filename
real(kind=8), intent(in) :: value
integer, intent(in) :: start_dp, end_dp, step
character(len=20) :: format_string
integer :: unit_num, i, line_no

unit_num = 10

open(unit=unit_num, file=filename, status='replace', &
   action='write')

 ! Loop over decimal places and include index as first column
line_no = 1
do i = start_dp, end_dp, step
   if (i == 0) then
      ! index and integer value
      write(unit_num, '(i0,2x,i0)') line_no, int(value)
   else
      ! build format like '(i0,2x,f<w>.<d>)' where w=i+2 and d=i
      write(format_string, '(a,i0,a,i0,a)') '(i0,2x,f', i+2, '.', i, ')'
      write(unit_num, format_string) line_no, value
   end if
   line_no = line_no + 1
end do

close(unit_num)

end subroutine write_precision_file

end program pi_precision_test
