#include "HighlightDialog.hpp"
#include "ui_HighlightDialog.h" // Include the generated UI header

#include <QColorDialog>
#include <QListWidgetItem>
#include <QMessageBox> // For potential error messages

HighlightDialog::HighlightDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::HighlightDialog),
    m_currentColor(Qt::yellow) // Default color for new rules
{
    ui->setupUi(this);
    updateColorButtonAppearance(); // Set initial button color

    // Connect signals for enabling/disabling edit/remove buttons
    connect(ui->listWidgetRules, &QListWidget::itemSelectionChanged,
            this, &HighlightDialog::on_listWidgetRules_itemSelectionChanged);

    // Initial state: disable edit/remove buttons
    updateButtonStates();
}

HighlightDialog::~HighlightDialog()
{
    delete ui;
}

void HighlightDialog::setHighlightRules(const QList<HighlightRule> &rules)
{
    m_rules = rules;
    populateListWidget();
}

QList<HighlightRule> HighlightDialog::getHighlightRules() const
{
    // The m_rules list is updated directly by add/edit/remove actions
    return m_rules;
}

void HighlightDialog::populateListWidget()
{
    ui->listWidgetRules->clear();
    for (const auto &rule : m_rules) {
        displayRuleInList(rule);
    }
    updateButtonStates(); // Update buttons after populating
}

void HighlightDialog::displayRuleInList(const HighlightRule &rule)
{
    QListWidgetItem *item = new QListWidgetItem(ui->listWidgetRules);
    item->setText(QString("'%1'").arg(rule.substring));
    item->setBackground(rule.color);
    // Set text color based on background brightness for readability
    qreal luminance = 0.2126 * rule.color.redF() + 0.7152 * rule.color.greenF() + 0.0722 * rule.color.blueF();
    item->setForeground(luminance > 0.5 ? Qt::black : Qt::white);
    // Store the rule index or substring in the item's data for retrieval
    item->setData(Qt::UserRole, rule.substring); // Using substring as identifier for now
    ui->listWidgetRules->addItem(item);
}


void HighlightDialog::on_buttonAddRule_clicked()
{
    QString substring = ui->lineEditSubstring->text().trimmed();
    if (substring.isEmpty()) {
        QMessageBox::warning(this, "Add Rule", "Substring cannot be empty.");
        return;
    }

    // Check for duplicates (optional, based on desired behavior)
    for(const auto& existingRule : m_rules) {
        if (existingRule.substring == substring) {
             QMessageBox::warning(this, "Add Rule", QString("Rule for substring '%1' already exists.").arg(substring));
             return;
        }
    }


    HighlightRule newRule(substring, m_currentColor);
    m_rules.append(newRule);
    displayRuleInList(newRule); // Add visually to the list
    ui->lineEditSubstring->clear(); // Clear input field
    updateButtonStates();
}

void HighlightDialog::on_buttonChooseColor_clicked()
{
    QColor chosenColor = QColorDialog::getColor(m_currentColor, this, "Choose Highlight Color");
    if (chosenColor.isValid()) {
        m_currentColor = chosenColor;
        updateColorButtonAppearance();
    }
}

void HighlightDialog::on_buttonEditRule_clicked()
{
    QListWidgetItem *selectedItem = ui->listWidgetRules->currentItem();
    if (!selectedItem) return;

    QString currentSubstring = selectedItem->data(Qt::UserRole).toString();
    int ruleIndex = -1;
    for(int i = 0; i < m_rules.size(); ++i) {
        if (m_rules[i].substring == currentSubstring) {
            ruleIndex = i;
            break;
        }
    }

    if (ruleIndex == -1) return; // Should not happen if list is synced

    // --- Simple Edit: Just update color ---
    // More complex edit (changing substring) would require more UI/logic
    QColor chosenColor = QColorDialog::getColor(m_rules[ruleIndex].color, this, "Edit Highlight Color");
    if (chosenColor.isValid()) {
        m_rules[ruleIndex].color = chosenColor;
        // Update item appearance
        selectedItem->setBackground(chosenColor);
        qreal luminance = 0.2126 * chosenColor.redF() + 0.7152 * chosenColor.greenF() + 0.0722 * chosenColor.blueF();
        selectedItem->setForeground(luminance > 0.5 ? Qt::black : Qt::white);
    }
}

void HighlightDialog::on_buttonRemoveRule_clicked()
{
    QListWidgetItem *selectedItem = ui->listWidgetRules->currentItem();
    if (!selectedItem) return;

    QString substringToRemove = selectedItem->data(Qt::UserRole).toString();

    // Remove from internal list
    bool removed = false;
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules[i].substring == substringToRemove) {
            m_rules.removeAt(i);
            removed = true;
            break;
        }
    }

    // Remove from list widget
    if (removed) {
        delete ui->listWidgetRules->takeItem(ui->listWidgetRules->row(selectedItem));
    }
    updateButtonStates();
}

void HighlightDialog::on_listWidgetRules_itemSelectionChanged()
{
    updateButtonStates();
}

void HighlightDialog::updateButtonStates()
{
    bool itemSelected = ui->listWidgetRules->currentItem() != nullptr;
    ui->buttonEditRule->setEnabled(itemSelected);
    ui->buttonRemoveRule->setEnabled(itemSelected);
}

void HighlightDialog::updateColorButtonAppearance()
{
    // Set the button's background color to show the selected color
    QString styleSheet = QString("background-color: %1; color: %2;")
                         .arg(m_currentColor.name())
                         .arg(m_currentColor.lightnessF() > 0.5 ? "black" : "white"); // Text color contrast
    ui->buttonChooseColor->setStyleSheet(styleSheet);
}