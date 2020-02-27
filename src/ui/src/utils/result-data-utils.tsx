import * as _ from 'lodash';
import {Column, Relation, RowBatchData} from 'types/generated/vizier_pb';

import {extractData} from '../components/chart/data';
import {formatUInt128} from './format-data';
import {nanoToMilliSeconds} from './time';

export function ResultsToCsv(results) {
  const jsonResults = JSON.parse(results);
  let csvStr = '';

  csvStr += _.map(jsonResults.relation.columns, 'columnName').join() + '\n';
  _.each(jsonResults.rowBatches, (rowBatch) => {
    const numRows = parseInt(rowBatch.numRows, 10);
    const numCols = rowBatch.cols.length;
    for (let i = 0; i < numRows; i++) {
      const rowData = [];
      for (let j = 0; j < numCols; j++) {
        const colKey = Object.keys(rowBatch.cols[j])[0];
        let data = rowBatch.cols[j][colKey].data[i];
        if (typeof data === 'string') {
          data = data.replace(/"/g, '\\\\\"\"');
          data = data.replace(/^{/g, '""{');
          data = data.replace(/}$/g, '}""');
          data = data.replace(/^\[/g, '""[');
          data = data.replace(/\[$/g, ']""');
        }
        rowData.push('"' + data + '"');
      }
      csvStr += rowData.join() + '\n';
    }
  });

  return csvStr;
}

export function ResultsToJSON(results) {
  let resValues = [];

  if (!results.rowBatches) {
    return resValues;
  }

  for (const batch of results.rowBatches) {
    const formattedBatch = [];
    for (let i = 0; i < parseInt(batch.numRows, 10); i++) {
      formattedBatch.push({});
    }

    batch.cols.forEach((col, i) => {
      const type = results.relation.columns[i].columnType;
      const name = results.relation.columns[i].columnName;

      const extractedData = extractData(type, col);

      extractedData.forEach((d, j) => {
        formattedBatch[j][name] = d;
      });
    });
    resValues = resValues.concat(formattedBatch);
  }

  return resValues;
}

function columnFromProto(column: Column): Array<{}> {
  if (column.hasBooleanData()) {
    return column.getBooleanData().getDataList();
  } else if (column.hasInt64Data()) {
    return column.getInt64Data().getDataList();
  } else if (column.hasUint128Data()) {
    const data = column.getUint128Data().getDataList();
    return data.map((d) => formatUInt128(String(d.getHigh()), String(d.getLow())));
  } else if (column.hasFloat64Data()) {
    return column.getFloat64Data().getDataList();
  } else if (column.hasStringData()) {
    return column.getStringData().getDataList();
  } else if (column.hasTime64nsData()) {
    const data = column.getTime64nsData().getDataList();
    return data.map(nanoToMilliSeconds);
  } else if (column.hasDuration64nsData()) {
    return column.getDuration64nsData().getDataList();
  }
  throw (new Error('Unsupported data type: ' + column.getColDataCase()));
}

export function dataFromProto(relation: Relation, data: RowBatchData[]) {
  const results = [];

  const colRelations = relation.getColumnsList();

  for (const batch of data) {
    const rows = [];
    for (let i = 0; i < batch.getNumRows(); i++) {
      rows.push({});
    }
    const cols = batch.getColsList();
    cols.forEach((col, i) => {
      const name = colRelations[i].getColumnName();
      columnFromProto(col).forEach((d, j) => {
        rows[j][name] = d;
      });
    });
    results.push(...rows);
  }
  return results;
}
