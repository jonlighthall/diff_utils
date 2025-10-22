module file_diff_utils
  ! Common utilities for file comparison programs
  ! Reduces code duplication across tldiff, tsdiff, prsdiff, cpddiff
  !
  ! Author: Refactored from individual diff programs
  ! Date: September 2025

  implicit none

  ! Define precision consistently across all programs
  integer, parameter :: srk = selected_real_kind(2)

  ! range
  real(kind=srk), dimension(:), allocatable :: r1,r2

   ! arguments
   integer :: ln1,ln2,ln3
   ! Command-line arguments:
   !   1st argument: filename for first time series file (reference)
   !   2nd argument: filename for second time series file (comparison)
   !   3rd argument: pass/fail threshold for time-level difference (optional)

  end module file_diff_utils

  module more_file_diff_utils
    use file_diff_utils
  ! Common parameters
  real(kind=srk), parameter :: rng_min_diff = 0.01  ! Minimum range difference

  ! ANSI color codes for consistent output
  character(len=*), parameter :: COLOR_RED = achar(27)//'[31m'
  character(len=*), parameter :: COLOR_GREEN = achar(27)//'[32m'
  character(len=*), parameter :: COLOR_BLUE = achar(27)//'[34m'
  character(len=*), parameter :: COLOR_CYAN = achar(27)//'[36m'
  character(len=*), parameter :: COLOR_YELLOW = achar(27)//'[33m'
  character(len=*), parameter :: COLOR_RESET = achar(27)//'[0m'

  ! File information type
  type :: file_info
    character(len=256) :: filename
    integer :: unit_number
    integer :: num_lines
    integer :: num_delimiters
    logical :: has_header = .false.
  end type file_info

contains

  !============================================================================
  ! COMMAND LINE PROCESSING
  !============================================================================

  subroutine get_diff_arguments(fname1, fname2, threshold, default1, default2, default_thresh)
    ! Get command line arguments with defaults
    character(len=*), intent(out) :: fname1, fname2
    real(kind=srk), intent(out) :: threshold
    character(len=*), intent(in) :: default1, default2
    real(kind=srk), intent(in) :: default_thresh

    character(len=256) :: arg_str
    integer :: arg_len

    ! Get filenames
    call get_command_argument(1, fname1, arg_len)
    if (arg_len == 0) fname1 = default1

    call get_command_argument(2, fname2, arg_len)
    if (arg_len == 0) fname2 = default2

    ! Get threshold
    call get_command_argument(3, arg_str, arg_len)
    if (arg_len > 0) then
      read(arg_str, *) threshold
      print *, 'using user-defined threshold'
    else
      threshold = default_thresh
      print *, 'using default threshold'
    endif
  end subroutine get_diff_arguments

  !============================================================================
  ! FILE ANALYSIS
  !============================================================================

  subroutine analyze_file_structure(file_info_obj, skip_header)
    ! Analyze file structure: count lines, delimiters, optionally skip header
    type(file_info), intent(inout) :: file_info_obj
    logical, intent(in), optional :: skip_header

    character(len=1024) :: line_buffer
    integer :: io_status, j, last_space, line_count, delimiter_count
    logical :: skip_first_line, first_line_processed

    skip_first_line = .false.
    if (present(skip_header)) skip_first_line = skip_header

    line_count = 0
    delimiter_count = 0
    first_line_processed = .false.

    do
      read(file_info_obj%unit_number, '(a)', iostat=io_status) line_buffer
      if (io_status /= 0) exit

      line_count = line_count + 1

      ! Check if we should skip the first line
      if (skip_first_line .and. line_count == 1) then
        ! Check if first line contains text (non-numeric)
        if (is_text_line(line_buffer)) then
          file_info_obj%has_header = .true.
          print *, 'Skipping header line: ', trim(line_buffer)
          cycle  ! Skip this line
        endif
      endif

      ! Count delimiters only on first data line
      if (.not. first_line_processed) then
        last_space = 0
        do j = 1, len(trim(line_buffer))
          if (line_buffer(j:j) == ' ') then
            if (j /= (last_space + 1)) then  ! Not consecutive spaces
              delimiter_count = delimiter_count + 1
            endif
            last_space = j
          endif
        enddo
        first_line_processed = .true.
      endif
    enddo

    ! Adjust line count if header was skipped
    if (file_info_obj%has_header) then
      line_count = line_count - 1
    endif

    file_info_obj%num_lines = line_count
    file_info_obj%num_delimiters = delimiter_count

    print '(1x,2a,i5,a,i3,a)', trim(file_info_obj%filename), ' has ', &
          line_count, ' lines and ', delimiter_count, ' delimiters'
  end subroutine analyze_file_structure

  logical function is_text_line(line)
    ! Check if a line contains text (non-numeric content)
    character(len=*), intent(in) :: line
    character(len=len(line)) :: test_line
    real :: dummy_real
    integer :: io_status

    test_line = adjustl(line)  ! Remove leading spaces

    ! Try to read the first word as a number
    read(test_line, *, iostat=io_status) dummy_real

    ! If read fails, it's likely text
    is_text_line = (io_status /= 0)

    ! Additional check: look for common header indicators
    if (index(test_line, '#') > 0 .or. &
        index(test_line, '!') > 0 .or. &
        index(test_line, 'range') > 0 .or. &
        index(test_line, 'time') > 0 .or. &
        index(test_line, 'freq') > 0) then
      is_text_line = .true.
    endif
  end function is_text_line

  !============================================================================
  ! FILE OPERATIONS
  !============================================================================

  subroutine open_file_safe(filename, unit_num, program_name)
    ! Safely open a file with error checking
    character(len=*), intent(in) :: filename, program_name
    integer, intent(out) :: unit_num
    integer :: io_status

    open(newunit=unit_num, file=filename, status='old', iostat=io_status)
    if (io_status /= 0) then
      print *, COLOR_RED // 'Error opening file: ' // trim(filename) // COLOR_RESET
      print *, 'I/O status code: ', io_status
      call exit_with_error(program_name)
    endif
  end subroutine open_file_safe

  subroutine close_files_safe(unit1, unit2)
    ! Safely close files with error handling
    integer, intent(in) :: unit1, unit2
    integer :: io_status

    write(*, '(1x,a)', advance='no') 'closing files... '

    close(unit1, iostat=io_status)
    if (io_status /= 0) then
      print *, COLOR_RED // 'error closing file 1' // COLOR_RESET
    endif

    close(unit2, iostat=io_status)
    if (io_status /= 0) then
      print *, COLOR_RED // 'error closing file 2' // COLOR_RESET
    endif

    print *, 'done'
  end subroutine close_files_safe

  !============================================================================
  ! FILE COMPARISON
  !============================================================================

  logical function compare_file_dimensions(file1, file2, program_name)
    ! Compare file dimensions and report results
    type(file_info), intent(in) :: file1, file2
    character(len=*), intent(in) :: program_name

    compare_file_dimensions = .true.

    ! Compare line counts
    if (file1%num_lines == file2%num_lines) then
      print '(1x,a,i5)', 'file lengths match: ', file1%num_lines
    else
      print *, 'file lengths do not match'
      print '(1x,a,i5)', 'length file 1 = ', file1%num_lines
      print '(1x,a,i5)', 'length file 2 = ', file2%num_lines
      compare_file_dimensions = .false.
      call exit_with_error(program_name)
      return
    endif

    ! Compare delimiter counts
    if (file1%num_delimiters == file2%num_delimiters) then
      print '(1x,a,i5)', 'number of file delimiters match: ', file1%num_delimiters
    else
      print *, 'number of file delimiters do not match'
      print '(1x,a,i5)', 'delim file 1 = ', file1%num_delimiters
      print '(1x,a,i5)', 'delim file 2 = ', file2%num_delimiters
      compare_file_dimensions = .false.
      call exit_with_error(program_name)
      return
    endif
  end function compare_file_dimensions

  logical function compare_ranges(r1, r2, n)
    ! Compare range arrays between two files
    integer, intent(in) :: n
    real(kind=srk), dimension(n), intent(in) :: r1, r2
    integer :: i

    compare_ranges = .true.

    do i = 1, n
      if (abs(r1(i) - r2(i)) > rng_min_diff) then
        print *, 'ranges do not match'
        print *, 'range ', i, ' file 1 = ', r1(i)
        print *, 'range ', i, ' file 2 = ', r2(i)
        compare_ranges = .false.
        return
      endif
    enddo

    print *, 'ranges match'
  end function compare_ranges

  !============================================================================
  ! OUTPUT AND ERROR HANDLING
  !============================================================================

  subroutine print_final_status(nerr3, fname1, fname2)
    ! Print final OK/ERROR status with consistent formatting
    character(len=*), intent(in) :: fname1, fname2
    integer, intent(in) :: nerr3

    print *, 'file1 = ', trim(fname1)
    print *, 'file2 = ', trim(fname2)

    if (nerr3 > 0) then
      print *, COLOR_RED // 'ERROR' // COLOR_RESET
      stop 1
    else
      print *, COLOR_GREEN // 'OK' // COLOR_RESET
    endif
  end subroutine print_final_status

  subroutine exit_with_error(program_name)
    ! Consistent error exit
    character(len=*), intent(in) :: program_name
    print *, COLOR_RED // '***ERROR***' // COLOR_RESET
    print *, 'stopping...'
    stop 1
  end subroutine exit_with_error

  subroutine print_threshold_info(label, value, color_code)
    ! Print threshold information with consistent formatting
    character(len=*), intent(in) :: label, color_code
    real(kind=srk), intent(in) :: value

    print '(a12,a,f7.3,a)', label, ' = ' // color_code, value, COLOR_RESET
  end subroutine print_threshold_info

end module more_file_diff_utils
