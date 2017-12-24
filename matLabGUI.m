%-----------------------------------------------------------------------
% desin_labTaz_spencer.m
% Written by Spencer McDermott and Taz Colanhelo
% Date: 2017 April 4
%-----------------------------------------------------------------------

function varargout = desin_labTaz_spencer(varargin)
% Begin initialization code - DO NOT EDIT
gui_Singleton = 1;
gui_State = struct('gui_Name',       mfilename, ...
                   'gui_Singleton',  gui_Singleton, ...
                   'gui_OpeningFcn', @desin_labTaz_spencer_OpeningFcn, ...
                   'gui_OutputFcn',  @desin_labTaz_spencer_OutputFcn, ...
                   'gui_LayoutFcn',  [] , ...
                   'gui_Callback',   []);
if nargin && ischar(varargin{1})
    gui_State.gui_Callback = str2func(varargin{1});
end

if nargout
    [varargout{1:nargout}] = gui_mainfcn(gui_State, varargin{:});
else
    gui_mainfcn(gui_State, varargin{:});
end
% End initialization code - DO NOT EDIT


% --- Executes just before desin_labTaz_spencer is made visible.
function desin_labTaz_spencer_OpeningFcn(hObject, eventdata, handles, varargin)
global comport
global RUN
global NPOINTS
RUN = 0;
NPOINTS = 2;
comport = serial('COM4','BaudRate',115200);

fopen(comport)
% Choose default command line output for myscope
handles.output = hObject;
% Update handles structure
guidata(hObject, handles);
set(handles.Run_Button,'string','RUN') %flag________________________

% --- Outputs from this function are returned to the command line.
function varargout = desin_labTaz_spencer_OutputFcn(hObject, eventdata, handles,varargin) 

% --- Executes on button press in Run_Button.
function Run_Button_Callback(~, ~, handles)
global comport
global NPOINTS
global RUN
global data 

if RUN == 0
    
  RUN = 1;
  % change the string on the button to STOP
  set(handles.Run_Button,'string','STOP')
  
  %clear plot
  hold off
  plot(0,0)
  
  %take program run cycle start time
  T_0 = datetime('now')
  
  %set x veiwing bounds for graph
  x_low = 0;
  x_high = 30;
  
  data = zeros(100000,2);
  while RUN == 1
    
    %get user entered temperature
    theSetTemp = str2double(get(handles.edit_temp, 'string'));
    %sends MSP-430 the temperature
    set_the_temperature(theSetTemp);
    
    % creates an array of pushed ADC value end to start and temperature
      
    [time, temp] = dynamic_array(T_0);
    
    % store time and temp values in data array 
    if(length(seconds(time)) == length(temp))
    data = [[seconds(time), temp]; data(1:100000,:)];
    end
    
    %display error and temp values in static text
    set(handles.display_temp, 'string', round(temp.*10)/10);
    set(handles.display_temp_error, 'string',  round(abs(theSetTemp - temp).*10)/10);
    %displays the graph

    [x_low, x_high] = display(time, temp, x_low, x_high); 
  
  end
else
  % data write to xlsx file 
  xlswrite('design_temp_data.xlsx',data)
  RUN = 0;
  
  %change the string on the button to RUN
  set(handles.Run_Button,'string','RUN')
  
end

% --- Executes on button press in Quit_Button.
function Quit_Button_Callback(hObject, eventdata, handles)
global comport
global RUN
global data
RUN = 0;
% data write to xlsx file 
xlswrite('design_temp_data.xlsx',data)
fclose(comport)
clear comport
% use close to terminate your program
% use quit to terminate MATLAB

close

function edit_temp_Callback(hObject, eventdata, handles)

function edit_temp_CreateFcn(hObject, eventdata, handles)

if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
    set(hObject,'BackgroundColor','white');
end

function [x_low, x_high] = display(time, temp, x_low, x_high)
    
    % shift screen right if data out of view 
    if(x_high < seconds(time))
         x_high = x_high + 30;
         x_low = x_low + 30;
    end
    % if vectors are the same length
    if(length(seconds(time)) == length(temp))
    % change colour based on temperature
    if(temp > 30)
    plot(seconds(time),temp,'r.')
    elseif(temp > 15)
    plot(seconds(time),temp,'y.')    
    else
    plot(seconds(time),temp,'b.')    
    end
    end
    % display graph axis
    xlabel('Time(s)')
    ylabel('temperature(°C)')
    axis ([x_low x_high -3 47])
    grid on
    hold on
    % use drawnow to update the figure
    drawnow 
    

function[time, temp] = dynamic_array(T_0)
    global comport
    global NPOINTS
    
    % fetch two 8-bit bytes and take time for this occurrence
    bytes_received = fread(comport,NPOINTS,'uint8');
    time = datetime('now') - T_0;
    valueADC = bitshift(bytes_received([2:2:end]),8) +  bytes_received([1:2:end-1]);
   
    %convert ADC steps to temperature in deg C
    temp =  6.57026007861345e-5.*(valueADC).^2 +0.015136513090065.*valueADC...
        - 15.6012551275939; 
    
    
function set_the_temperature(theSetTemp)
   global comport
   
    % send a two character to the MCU for the (__.0) high order and the other the
    % (0._)lower order temperature
   
    decimalVal  = round((theSetTemp  - floor(theSetTemp)) .* 10);
    tempInBytes = [char(uint8(theSetTemp - decimalVal./10)), char(uint8(decimalVal))];
   
    fprintf(comport,'%s',tempInBytes(1)); 
    fprintf(comport,'%s',tempInBytes(2)); 
            
