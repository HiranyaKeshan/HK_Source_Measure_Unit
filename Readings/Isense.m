% Range 1: ±20 μA
current1_uA = -20:2:20;
vsense1_V = [1.862, 1.93, 1.997, 2.065, 2.133, 2.201, 2.269, 2.337, 2.404, 2.472, 2.54, 2.608, 2.675, 2.743, 2.811, 2.878, 2.946, 3.014, 3.081, 3.148, 3.215];
x1_norm = current1_uA / 20;

% Range 2: ±200 μA
current2_uA = -200:20:200;
vsense2_V = [1.846, 1.913, 1.979, 2.046, 2.113, 2.179, 2.246, 2.313, 2.379, 2.446, 2.513, 2.579, 2.645, 2.712, 2.778, 2.845, 2.912, 2.979, 3.046, 3.113, 3.180];
x2_norm = current2_uA / 200;

% Range 3: ±2 mA
current3_mA = -2:0.2:2;
vsense3_V = [1.781, 1.853, 1.925, 1.997, 2.069, 2.141, 2.213, 2.285, 2.358, 2.43, 2.502, 2.574, 2.646, 2.718, 2.789, 2.86, 2.93, 3, 3.07, 3.139, 3.209];
x3_norm = current3_mA / 2;

% Range 4: ±20 mA
current4_mA = -20:2:20;
vsense4_V = [1.837, 1.903, 1.969, 2.034, 2.101, 2.167, 2.233, 2.299, 2.365, 2.431, 2.497, 2.563, 2.629, 2.696, 2.762, 2.828, 2.893, 2.96, 3.025, 3.091, 3.157];
x4_norm = current4_mA / 20;

% Range 5: ±200 mA
current5_mA = -200:20:200;
vsense5_V = [1.899, 1.962, 2.023, 2.082, 2.142, 2.202, 2.261, 2.321, 2.38, 2.44, 2.506, 2.566, 2.626, 2.685, 2.745, 2.805, 2.865, 2.925, 2.985, 3.045, 3.108];
x5_norm = current5_mA / 200;

% Range 6: ±2 A (data available only for ±1 A)
current6_A = -2:0.2:2;
vsense6_V = [NaN, NaN, NaN, NaN, NaN, 2.261, 2.308, 2.354, 2.4, 2.447, 2.493, 2.54, 2.587, 2.633, 2.68, 2.726, NaN, NaN, NaN, NaN, NaN];
x6_norm = current6_A / 2;

colors = {'r', 'g', 'b', 'c', 'm', 'k'};

range_labels = {'\pm20 \muA range', '\pm200 \muA range', '\pm2 mA range', '\pm20 mA range', '\pm200 mA range', '\pm2 A range'};

max_displays = [20, 200, 2, 20, 200, 2];
unit_strs = {'\muA', '\muA', 'mA', 'mA', 'mA', 'A'};
format_specs = {'%.0f', '%.0f', '%.1f', '%.0f', '%.0f', '%.1f'};

aux_height = 0.035;
aux_space = 0.005;
num_aux = 6;
total_aux_height = num_aux * aux_height + (num_aux - 1) * aux_space;
main_bottom = 0.05 + total_aux_height;
main_height = 0.9 - main_bottom;
left = 0.1;
width = 0.8;

x_norms = {x1_norm, x2_norm, x3_norm, x4_norm, x5_norm, x6_norm};
vsenses = {vsense1_V, vsense2_V, vsense3_V, vsense4_V, vsense5_V, vsense6_V};

figure(1);
ax_main1 = axes('Position', [left, main_bottom, width, main_height]);
hold on;
for i = 1:6
    x = x_norms{i};
    y = vsenses{i};
    plot(x, y, [colors{i} '-o'], 'LineWidth', 1.5, 'MarkerSize', 4);
    % Best fit line (handling NaNs)
    valid = ~isnan(y);
    if sum(valid) >= 2
        p = polyfit(x(valid), y(valid), 1);
        x_fit = linspace(min(x(valid)), max(x(valid)), 2);
        y_fit = polyval(p, x_fit);
        h_fit = plot(x_fit, y_fit, '--', 'Color', colors{i}, 'LineWidth', 1.5);
        set(h_fit, 'HandleVisibility', 'off');
    end
end
xline(0, 'k--', 'LineWidth', 1.5);
yline(2.5, 'k--', 'LineWidth', 1.5);

hold off;
ylabel('ISense (V)');
ylim(ax_main1, [1.5 3.5]);
title('ISense Voltage vs Input Current');
legend(range_labels, 'Location', 'best');
grid on;
set(ax_main1, 'XTick', [], 'XAxisLocation', 'top', 'XColor', 'none');

current_bottom = main_bottom - aux_height;
for i = 1:num_aux
    ax_aux(i) = axes('Position', [left, current_bottom, width, aux_height]);
    set(ax_aux(i), 'XLim', [-1 1], 'YTick', [], 'YColor', 'none', 'Box', 'off', 'Color', 'none', 'XAxisLocation', 'bottom');
    set(ax_aux(i), 'XTick', linspace(-1, 1, 11));
    tick_labels = compose(format_specs{i}, linspace(-max_displays(i), max_displays(i), 11));
    if i == num_aux
        tick_labels{6} = ['0 (' unit_strs{i} ')'];
        xlabel(ax_aux(i), {'Input Current', ['(' unit_strs{i} ')']}, 'Interpreter', 'none');
    else
        tick_labels{6} = ['0 (' unit_strs{i} ')'];
    end
    set(ax_aux(i), 'XTickLabel', tick_labels);
    current_bottom = current_bottom - aux_height - aux_space;
end

%% ---- Linearity and Offset Table ----

num_ranges = 6;

Offsets = zeros(num_ranges,1);
Max_NL_percent = zeros(num_ranges,1);

for i = 1:num_ranges
    
    x = x_norms{i};
    y = vsenses{i};
    
    valid = ~isnan(y);
    
    if sum(valid) >= 2
        
        p = polyfit(x(valid), y(valid), 1);
        slope = p(1);
        
        idx_zero = find(x == 0);
        
        if ~isempty(idx_zero) && ~isnan(y(idx_zero))
            offset = y(idx_zero) - 2.5;
        else
            y_zero = polyval(p, 0);
            offset = y_zero - 2.5;
        end
        
        y_fit = polyval(p, x(valid));
        
        residual = y(valid) - y_fit;
        
        y_fit_min = polyval(p, min(x(valid)));
        y_fit_max = polyval(p, max(x(valid)));
        FS_span = abs(y_fit_max - y_fit_min);
        
        NL_percent = (max(abs(residual)) / FS_span) * 100;
        
        Offsets(i) = offset;
        Max_NL_percent(i) = NL_percent;
        
    else
        Offsets(i) = NaN;
        Max_NL_percent(i) = NaN;
    end
end

Linearity_Table = table(range_labels', Offsets, Max_NL_percent, ...
    'VariableNames', {'Current_Range', 'Offset_at_0A_V', 'Max_Nonlinearity_percentFS'});

disp('Linearity and Offset Summary:')
disp(Linearity_Table)