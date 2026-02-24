% Range 1: ±200 mV
voltage1_mV = -200:20:200;
applied1_V = voltage1_mV / 1000;
output1_V = [1.37, 1.454, 1.536, 1.619, 1.703, 1.785, 1.868, ...
             1.952, 2.035, 2.118, 2.201, 2.285, 2.368, ...
             2.451, 2.533, 2.616, 2.7, 2.784, 2.859, ...
             2.942, 3.025];
x1_norm = voltage1_mV / 200;

% Range 2: ±2 V
voltage2_mV = -2000:200:2000;
applied2_V = voltage2_mV / 1000;
output2_V = [1.567, 1.658, 1.749, 1.84, 1.931, 2.022, ...
             2.113, 2.204, 2.295, 2.386, 2.477, 2.568, ...
             2.659, 2.75, 2.841, 2.933, 3.024, 3.115, ...
             3.206, 3.297, 3.388];
x2_norm = voltage2_mV / 2000;

% Range 3: ±12 V
voltage3_V = -12:1:12;
applied3_V = voltage3_V;
output3_V = [1.535, 1.616, 1.696, 1.777, 1.857, 1.937, ...
             2.018, 2.098, 2.179, 2.259, 2.34, 2.42, ...
             2.501, 2.581, 2.662, 2.742, 2.823, 2.903, ...
             2.984, 3.064, 3.1446, 3.225, 3.305, 3.386, 3.466];
x3_norm = voltage3_V / 12;

colors = {'r', 'g', 'b'};
range_labels = {'\pm200 mV range', '\pm2 V range', '\pm12 V range'};

max_displays = [200, 2, 12];
unit_strs = {'mV', 'V', 'V'};
format_specs = {'%.0f', '%.1f', '%.0f'};

aux_height = 0.035;
aux_space = 0.005;
num_aux = 3;
total_aux_height = num_aux * aux_height + (num_aux - 1) * aux_space;
main_bottom = 0.05 + total_aux_height;
main_height = 0.9 - main_bottom;
left = 0.1;
width = 0.8;

x_norms = {x1_norm, x2_norm, x3_norm};
applieds = {applied1_V, applied2_V, applied3_V};
outputs = {output1_V, output2_V, output3_V};

figure(2);
ax_main2 = axes('Position', [left, main_bottom, width, main_height]);
hold on;

for i = 1:3
    x = x_norms{i};
    y = outputs{i};
    plot(x, y, [colors{i} '-o'], 'LineWidth', 1.5, 'MarkerSize', 3);

    p = polyfit(x, y, 1);
    x_fit = linspace(min(x), max(x), 2);
    y_fit = polyval(p, x_fit);
    h_fit = plot(x_fit, y_fit, '--', 'Color', colors{i}, 'LineWidth', 1.5);
    set(h_fit, 'HandleVisibility', 'off');
end

xline(0, 'k--', 'LineWidth', 1.5);
yline(2.5, 'k--', 'LineWidth', 1.5);

hold off;

ylabel('Vsense (V)');
title('VSense Voltage vs Input Voltage');
legend(range_labels, 'Location', 'best');
grid on;
set(ax_main2, 'XTick', [], 'XAxisLocation', 'top', 'XColor', 'none');

current_bottom = main_bottom - aux_height;

for i = 1:num_aux
    ax_aux(i) = axes('Position', [left, current_bottom, width, aux_height]);
    set(ax_aux(i), 'XLim', [-1 1], 'YTick', [], ...
        'YColor', 'none', 'Box', 'off', ...
        'Color', 'none', 'XAxisLocation', 'bottom');

    if i == 3
        ticks_display = -12:2:12;            
        ticks_norm = ticks_display / 12;    
        set(ax_aux(i), 'XTick', ticks_norm);
        tick_labels = compose('%.0f', ticks_display);
    else
        set(ax_aux(i), 'XTick', linspace(-1, 1, 11));
        tick_labels = compose(format_specs{i}, ...
            linspace(-max_displays(i), max_displays(i), 11));
    end

for j = 1:length(tick_labels)
    tick_value = str2double(tick_labels{j});
    if ~isnan(tick_value) && abs(tick_value) < 1e-10
        tick_labels{j} = ['0 (' unit_strs{i} ')'];
        break;
    end
end

    if i == num_aux
        xlabel(ax_aux(i), {'Input Voltage', ['(' unit_strs{i} ')']}, ...
            'Interpreter', 'none');
    end

    set(ax_aux(i), 'XTickLabel', tick_labels);
    current_bottom = current_bottom - aux_height - aux_space;
end

%% ---- VSense Linearity and Offset Table ----

num_ranges = 3;

Offsets = zeros(num_ranges,1);
Max_NL_percent = zeros(num_ranges,1);

for i = 1:num_ranges
    
    x = x_norms{i};   
    y = outputs{i};  
   
    p = polyfit(x, y, 1);
    slope = p(1);
    
    idx_zero = find(x == 0);
    
    if ~isempty(idx_zero)
        offset = y(idx_zero) - 2.5; 
    else
        y_zero = polyval(p, 0);
        offset = y_zero - 2.5;
    end
    
    y_fit = polyval(p, x);
    
    residual = y - y_fit;
    
    y_fit_min = polyval(p, min(x));
    y_fit_max = polyval(p, max(x));
    FS_span = abs(y_fit_max - y_fit_min);
    
    NL_percent = (max(abs(residual)) / FS_span) * 100;
    
    Offsets(i) = offset;
    Max_NL_percent(i) = NL_percent;
end

VSense_Linearity_Table = table(range_labels', Offsets, Max_NL_percent, ...
    'VariableNames', {'Voltage_Range', 'Offset_at_0V_V', 'Max_Nonlinearity_percentFS'});

disp('VSense Linearity and Offset Summary:')
disp(VSense_Linearity_Table)